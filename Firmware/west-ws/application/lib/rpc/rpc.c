#include <lib/rpc/rpc.h>
#include <Protos/v1/rpc.h>

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rpc_lib, LOG_LEVEL_ERR);

enum rpc_event {
    RPC_EVENT_RESPONSE = BIT(0),
};

union message_data_u {
    Request request;
    Cancel cancel;
    Response response;
};

static void workq_handler(struct k_work *item);

int rpc_register_function(struct rpc *rpc, int function_id, rpc_function_t function, uint32_t result_type)
{
    if (rpc->function_count >= CONFIG_RPC_FUNCTION_COUNT) return -ENOMEM;

    // Make sure that the function ID is unique
    for (uint8_t func_table_idx = 0; func_table_idx < rpc->function_count; func_table_idx++)
    {
        struct rpc_function *func = &rpc->functions[func_table_idx];
        if (func->function_id == function_id) return -EINVAL;
    }

    struct rpc_function *func = &rpc->functions[rpc->function_count++];
    func->function_id = function_id;
    func->function = function;
    func->result_type = result_type;

    return 0;
}

int rpc_register_target(struct rpc *rpc, int target_id, const struct rpc_io_api_s *io_api)
{
    if (rpc->rpc_target_count >= CONFIG_RPC_TARGET_COUNT) return -ENOMEM;

    // Make sure that the target ID is unique
    for (uint8_t target_table_idx = 0; target_table_idx < rpc->rpc_target_count; target_table_idx++)
    {
        struct rpc_target *target = &rpc->targets[target_table_idx];
        if (target->target_id == target_id) return -EINVAL;
    }

    struct rpc_target *target = &rpc->targets[rpc->rpc_target_count++];
    target->target_id = target_id;
    target->io_api = io_api;
    target->user_data = NULL;

    // Also reset all of the target's state
    target->last_recv_message_id = 0;
    target->last_sent_message_id = 0;
    target->rx_channels_mask = 0;
    target->tx_channels_mask = 0;
    for (uint8_t channel_idx = 0; channel_idx < CONFIG_RPC_CONCURRENT_REQUESTS; channel_idx++)
    {
        target->rx_channels[channel_idx].message = NULL;
        target->tx_channels[channel_idx].message = NULL;
    }

    return rpc->rpc_target_count - 1;
}

static void workq_handler(struct k_work *item)
{
    Message response_message = Message_init_zero;
    Response *response = &response_message.data.response;
    uint32_t response_code = 0;

    struct rpc_rx_channel *channel = CONTAINER_OF(item, struct rpc_rx_channel, work);
    Message *message = channel->message;
    struct rpc *rpc = channel->rpc;
    struct rpc_target *client = channel->client;

    response_message.has_m_id = true;
    response_message.m_id = client->target_id;
    response_message.has_rpc_id = true;
    response_message.rpc_id = message->rpc_id;
    response_message.which_data = Message_response_tag;

    Request *request = &message->data.request;

    // Make sure the function ID is valid
    if (!request->has_func) return; // -EINVAL;
    struct rpc_function *func = NULL;
    for (uint8_t func_table_idx = 0; func_table_idx < rpc->function_count; func_table_idx++)
    {
        struct rpc_function *f = &rpc->functions[func_table_idx];
        if (f->function_id == request->func)
        {
            func = f;
            break;
        }
    }
    // if (func == NULL) return -EINVAL;
    if (func == NULL)
    {
        response_code = EINVAL;
        goto end;
    }

    // Call the function
    response_code = func->function(&request->args, &response->result);

    end:
    {
        if (response_code == 0)
        {
            response->has_type = true;
            response->type = ResponseType_SUCCESS;
            response->has_result = true;
        }
        else
        {
            response->has_type = true;
            response->type = ResponseType_ERROR;
            response->has_result = true;
            response->result.which_value = InOut_error_tag;
            response->result.value.error.has_code = true;
            response->result.value.error.code = response_code;
        }

        client->io_api->write(rpc, client, &response_message);

        // Release the channel
        // Find the channel number
        uint8_t channel_idx = 0;
        for (; channel_idx < CONFIG_RPC_CONCURRENT_REQUESTS; channel_idx++)
        {
            if (&client->rx_channels[channel_idx].work == item) break;
        }
        if (channel_idx == CONFIG_RPC_CONCURRENT_REQUESTS) k_panic();
        client->rx_channels_mask &= ~(1 << channel_idx);
        memset(channel, 0, sizeof(struct rpc_rx_channel));

        pb_release(Message_fields, message);
        free(message);
    }
}

static int rpc_post_request(struct rpc *rpc, uint32_t client_id, Message *message)
{
    // Assume that the caller has already validated the message header
    // Request *request = &message->data.request;

    // Look up the client that sent the message
    struct rpc_target *client = NULL;
    for (uint8_t target_table_idx = 0; target_table_idx < rpc->rpc_target_count; target_table_idx++)
    {
        struct rpc_target *target = &rpc->targets[target_table_idx];
        if (target->target_id == client_id)
        {
            client = target;
            break;
        }
    }
    if (client == NULL)
    {
        LOG_ERR("Received request from unknown client %d", client_id);
        pb_release(Message_fields, message);
        k_free(message);
        return -EINVAL;
    }
    
    // Make a new channel for this request
    // Find the first free channel
    uint8_t channel_idx = 0;
    for (; channel_idx < CONFIG_RPC_CONCURRENT_REQUESTS; channel_idx++)
    {
        bool channel_is_active = client->rx_channels_mask & (1 << channel_idx);
        if (!channel_is_active) break;
    }
    if (channel_idx == CONFIG_RPC_CONCURRENT_REQUESTS)
    {
        LOG_ERR("Client %d has too many active requests!", client_id);
        pb_release(Message_fields, message);
        k_free(message);
        return -ENOMEM;
    }

    // Set up the channel
    client->rx_channels_mask |= (1 << channel_idx);
    struct rpc_rx_channel *channel = &client->rx_channels[channel_idx];
    channel->message = message;
    channel->rpc = rpc;
    channel->client = client;
    k_work_init(&channel->work, workq_handler);
    k_work_submit(&channel->work);

    return 0;
}

static int rpc_post_cancel(struct rpc *rpc, uint32_t client_id, Message *message)
{
    // Assume that the caller has already validated the message

    pb_release(Message_fields, message);
    k_free(message);

    return 0;
}

static int rpc_post_response(struct rpc *rpc, uint32_t client_id, Message *message)
{
    // Assume that the caller has already validated the message
    // One of the clients has finished processing a request
    // and has sent the result to this device. The result needs to
    // be forwarded to the calling thread and released.
    //
    // The calling thread is waiting on the `events` event group
    // for the `RPC_EVENT_RESPONSE` bit to be set.

    // Look up the channel for this existing request
    uint8_t client_idx = 0;
    for (; client_idx < rpc->rpc_target_count; client_idx++)
    {
        if (rpc->targets[client_idx].target_id == client_id) break;
    }
    if (client_idx == rpc->rpc_target_count)
    {
        LOG_ERR("Response has invalid client ID: %d", client_id);
        pb_release(Message_fields, message);
        k_free(message);
        return -EINVAL;
    }
    struct rpc_target *client = &rpc->targets[client_idx];

    // Find the channel number
    uint8_t channel_idx = 0;
    for (; channel_idx < CONFIG_RPC_CONCURRENT_REQUESTS; channel_idx++)
    {
        if (!(client->tx_channels_mask & BIT(channel_idx))) continue;
        if (client->tx_channels[channel_idx].message == NULL) continue;
        if (client->tx_channels[channel_idx].message->rpc_id == message->rpc_id)
        {
            break;
        }
        else
        {
            printk("Unmatched RPC Channel ID: %d\n", client->tx_channels[channel_idx].message->rpc_id);
        }
    }
    if (channel_idx == CONFIG_RPC_CONCURRENT_REQUESTS)
    {
        LOG_ERR("Response has invalid RPC ID: %d. Active channels: 0x%02X", message->rpc_id, client->tx_channels_mask);
        pb_release(Message_fields, message);
        k_free(message);
        return -EINVAL;
    }
    struct rpc_tx_channel *channel = &client->tx_channels[channel_idx];

    // Copy the result
    // memcpy(channel->result, &message->data.response.result, sizeof(channel->result));
    channel->response = message;
    k_event_set(&channel->events, RPC_EVENT_RESPONSE);

    return 0;
}

int rpc_post(struct rpc *rpc, uint32_t client_id, Message *message)
{
    // Protobuf recommends leaving all fields optional
    // and validating everything in software
    // Let's enforce our own rules here
    int rc = message_validate(message);
    if ((rc != 0) && (rc != -Message_checksum_tag))
    {
        LOG_ERR("Received invalid message from client %d: %d", client_id, rc);
        return rc;
    }

    // Validate the message
    // Make sure we're the intended target
    if (message->target != rpc->id)
    {
        LOG_ERR("Received message for target %d, but we are %d", message->target, rpc->id);
        LOG_ERR("RPC: %p", rpc);

        return -EINVAL;
    }

    // Look up the client that sent the message
    struct rpc_target *client = NULL;
    for (uint8_t target_table_idx = 0; target_table_idx < rpc->rpc_target_count; target_table_idx++)
    {
        struct rpc_target *target = &rpc->targets[target_table_idx];
        if (target->target_id == client_id)
        {
            client = target;
            break;
        }
    }
    if (client == NULL)
    {
        LOG_ERR("Received message from unknown client %d", client_id);
        return -EINVAL;
    }

    // Make sure the message ID is one greater than the last received message ID
    uint32_t message_id = message->m_id;
    bool message_is_next = message_id == client->last_recv_message_id + 1;
    bool message_is_first = message_id == 1;

    client->last_recv_message_id = message_id;
    if (!(message_is_next || message_is_first))
    {
        LOG_ERR("Received message with invalid message ID: %d", message_id);
        return -EINVAL;
    }

    // uint32_t rpc_id =  message->rpc_id;
    // bool rpc_id_exists = false;
    // for (uint8_t channel_idx = 0; channel_idx < CONFIG_RPC_CONCURRENT_REQUESTS; channel_idx++)
    // {
    //     bool channel_is_active = client->rx_channels_mask & (1 << channel_idx);
    //     if (!channel_is_active) continue;

    //     struct rpc_rx_channel *channel = &client->rx_channels[channel_idx];
    //     rpc_id_exists |= channel->message->rpc_id == rpc_id;
    // }

    // Managed by the cooresponding `rpc_post_*` function,
    /// which will typically free it sometime later
    Message *message_copy = k_calloc(1, sizeof(Message));
    if (message_copy == NULL)
    {
        LOG_ERR("Could not allocate memory for message copy!");
        return -ENOMEM;
    }
    if (!pb_copy(Message_fields, message, message_copy))
    {
        LOG_ERR("Could not copy message!");
        k_free(message_copy);
        return -ENOMEM;
    }

    // Validate the RPC ID
    if (message->which_data == Message_request_tag)
    {
        // if (rpc_id_exists) return -EINVAL;
        return rpc_post_request(rpc, client_id, message_copy);
    }
    else if (message->which_data == Message_cancel_tag)
    {
        // if (!rpc_id_exists) return -EINVAL;
        return rpc_post_cancel(rpc, client_id, message_copy);
    }
    else if (message->which_data == Message_response_tag)
    {
        // if (!rpc_id_exists) return -EINVAL;
        return rpc_post_response(rpc, client_id, message_copy);
    }
    else
    {
        LOG_ERR("Received message with invalid data type: %d", message->which_data);
        pb_release(Message_fields, message_copy);
        k_free(message_copy);
        return -EINVAL;
    }

    return 0;
}

static struct rpc_tx_channel *rpc_send_message(struct rpc *rpc, uint32_t target_id, uint32_t which_data, union message_data_u *request)
{
    // Look up the client that will receive the message
    struct rpc_target *client = NULL;
    for (uint8_t target_table_idx = 0; target_table_idx < rpc->rpc_target_count; target_table_idx++)
    {
        struct rpc_target *target = &rpc->targets[target_table_idx];
        if (target->target_id == target_id)
        {
            client = target;
            break;
        }
    }
    if (client == NULL) return NULL;

    // Find the first free channel
    uint8_t channel_idx = 0;
    for (; channel_idx < CONFIG_RPC_CONCURRENT_REQUESTS; channel_idx++)
    {
        bool channel_is_active = client->tx_channels_mask & BIT(channel_idx);
        if (!channel_is_active) break;
    }
    if (channel_idx == CONFIG_RPC_CONCURRENT_REQUESTS) return NULL;

    // Message message = Message_init_zero;
    Message *message = k_calloc(1, sizeof(Message));
    message->version = 1;

    message->has_m_id = true;
    message->m_id = client->last_sent_message_id + 1;
    client->last_sent_message_id = message->m_id;

    message->has_rpc_id = true;
    message->rpc_id = client->last_sent_rpc_id + 1;
    client->last_sent_rpc_id = message->rpc_id;

    message->has_source = true;
    message->source = rpc->id;

    message->has_target = true;
    message->target = client->target_id;

    message->which_data = which_data;
    memcpy(&message->data, request, sizeof(message->data));

    // Set up the channel
    client->tx_channels_mask |= (1 << channel_idx); // Claim the channel
    LOG_ERR("Claimed channel %d of target %d\n", channel_idx, target_id);
    struct rpc_tx_channel *channel = &client->tx_channels[channel_idx];
    channel->message = message;
    k_event_init(&channel->events);

    client->io_api->write(rpc, client, channel->message);

    return channel;
}

static struct rpc_tx_channel *rpc_send_request(struct rpc *rpc, uint32_t target_id, Request *request)
{
    union message_data_u message_data;
    message_data.request = *request;
    return rpc_send_message(rpc, target_id, Message_request_tag, &message_data);
}

int rpc_call(struct rpc *rpc, uint32_t target_id, uint32_t function, const InOut *args, InOut *result, k_timeout_t timeout)
{

    // LOG_ERR("rpc_call(rpc: %p, target: %d, func: %d, args: %p, result: %p, timeout: %lld)", rpc, target_id, function, args, result, timeout.ticks);

    Request request = Request_init_zero;
    request.has_func = true;
    request.func = function;

    request.has_args = true;

    pb_copy(InOut_fields, args, &request.args);
    
    // TODO: This gets messy when the arguments are a repeated field,
    // and is a pointer (to somewhere on the stack or the heap)
    // There's no way that we can know how to manage the memory, so
    // it must be managed by the caller
    // This gets challenging because the caller may exit the scope
    // where the memory was allocated, and the RPC handler may not
    // have finished processing the request yet
    // We can't just copy the data because we shouldn't be concerned
    // about the type of argument at this level
    // We could iterate through the Message field list and see if the
    // argument type is a pointer, and if so, copy the pointer and
    // the size of the data it points to into the Message
    // Then, the RPC handler can free the memory when it's done
    // processing the request
    
    // For now, we'll make the caller clean it up, and just block
    // until the request is done

    // memcpy(&request.args, arg, arg_size);

    struct rpc_tx_channel *tx = rpc_send_request(rpc, target_id, &request);
    if (tx == NULL)
    {
        LOG_ERR("Could not send request to target %d", target_id);
        pb_release(Request_fields, &request);
        return -EINVAL;
    }

    // Wait until we receive the final response from the target
    // In the future there may be other events that we need to handle
    uint32_t terminating_events = RPC_EVENT_RESPONSE;
    uint32_t matching_events = 0;
    do
    {
        matching_events = k_event_wait(&tx->events, 0xFFFF, true, timeout);

        // TODO: Handle events that don't free the channel, like progress updates (future)
    } while (!(matching_events & terminating_events) && (matching_events != 0));

    if (matching_events == 0)
    {
        // We did not receive a response in time from the target
        // We'll let the caller know that there was an error by
        // updating the `error` field of the result
        result->which_value = InOut_error_tag;
        result->value.error.has_code = true;
        result->value.error.code = ETIMEDOUT;
    }
    else
    {
        // We received a response from the target
        // We'll just forward it to the caller
        if (!pb_copy(InOut_fields, &tx->response->data.response.result, result))
        {
            LOG_ERR("[rpc::call] Could not copy result!");
        }

        // printk("\n\nOriginal Result: \n");
        // InOut_print(&tx->response->data.response.result, 0);

        // printk("\n\nCopied Result: \n");
        // InOut_print(result, 0);
    }

    // Now we need to cleanup the channel and memory
    pb_release(Message_fields, tx->message);
    k_free(tx->message);
    tx->message = NULL;

    pb_release(Message_fields, tx->response);
    k_free(tx->response);
    tx->response = NULL;

    // Find the channel
    // First need to find the target
    uint8_t target_idx = 0;
    for (; target_idx < rpc->rpc_target_count; target_idx++)
    {
        if (rpc->targets[target_idx].target_id == target_id) break;
    }
    if (target_idx == rpc->rpc_target_count)
    {
        LOG_ERR("Could not find target %d", target_id);
        k_panic();
    }

    uint8_t channel_idx = 0;
    for (; channel_idx < CONFIG_RPC_CONCURRENT_REQUESTS; channel_idx++)
    {
        if (&rpc->targets[target_idx].tx_channels[channel_idx] == tx) break;
    }
    if (channel_idx == CONFIG_RPC_CONCURRENT_REQUESTS)
    {
        LOG_ERR("Could not find channel for target %d", target_id);
        k_panic();
    }

    rpc->targets[channel_idx].tx_channels_mask &= ~BIT(channel_idx);
    LOG_ERR("Freed channel %d of target %d\n", channel_idx, target_id);
    if (!(matching_events & RPC_EVENT_RESPONSE)) return -ETIMEDOUT;

    return 0;
}