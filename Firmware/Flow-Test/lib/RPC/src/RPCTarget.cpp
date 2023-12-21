#include "RPC.h"

#include <stdlib.h>
#include <exception>
#include <errno.h>

#include <pb_decode.h>
#include <pb_encode.h>


#define DEBUG(fmt, ...) if (this->_debug_port != NULL) this->_debug_port->printf(fmt, ##__VA_ARGS__)


RPCTarget::RPCTarget(RPCController *controller, uint32_t id, RPCIO* io) : _controller(controller), _id(id), _io(io) {}
RPCTarget::~RPCTarget() {}


rpc_tx_channel_t* RPCTarget::SendMessage(int tag, const message_data_t *data)
{
    Message *message = new Message(); // Free'd at the end of this function (copied by RPCIO::Write)
    if (!message) return NULL;

    message->version = 1;

    message->has_m_id = true;
    message->m_id = this->last_sent_message_id + 1;
    this->last_sent_message_id = message->m_id;

    message->has_rpc_id = true;
    message->rpc_id = this->last_sent_rpc_id + 1;
    this->last_sent_rpc_id = message->rpc_id;

    message->has_source = true;
    message->source = this->_controller->get_id();

    message->has_target = true;
    message->target = this->_id;

    message->which_data = tag;
    switch (tag)
    {
        case Message_request_tag:
            pb_copy(Request_fields, &data->request, &message->data.request);
            break;
        case Message_cancel_tag:
            pb_copy(Cancel_fields, &data->cancel, &message->data.cancel);
            break;
        case Message_response_tag:
            pb_copy(Response_fields, &data->response, &message->data.response);
            break;
        default:
        {
            DEBUG("Invalid message tag: %d\n", tag);
            delete message; // don't need pb_release, because we didn't decode/copy anything
            return NULL;
        }
    }
    // memcpy(&message->data, data, sizeof(message->data));

    rpc_tx_channel_t *channel = new rpc_tx_channel_t(); // Free'd by the caller (TODO!)
    if (!channel)
    {
        DEBUG("Failed to allocate channel\n");
        pb_release(Message_fields, message);
        delete message;
        return NULL;
    }

    int rc = this->_io->Write(message);
    if (rc == 0)
    {
        channel->request = message;
        channel->response = NULL;
        channel->flags = 0;
        channel->timeout_ms = 0;
    }
    else
    {
        delete channel;

        message = NULL;
        channel = NULL;
    }
    
    pb_release(Message_fields, message);
    delete message;

    return channel;
}

rpc_tx_channel_t* RPCTarget::SendRequest(const Request *request)
{
    message_data_t message_data;
    message_data.request = *request;
    return this->SendMessage(Message_request_tag, &message_data);
}

/**
 * @brief Sends a mesage to the target to invoke a function
 * @param function_id The function ID to invoke
 * @param arg The argument to pass to the function
 * @param result Where to store the result of the function
 * @param timeout_ms The timeout in milliseconds
 * @return The RPC ID on success (identifies this request), negative on error
 */
int RPCTarget::Call(FUNCTION function_id, const InOut *args, InOut *result, uint32_t timeout_ms)
{
    Request request = Request_init_zero;
    
    request.has_func = true;
    request.func = function_id;

    request.has_args = true;
    pb_copy(InOut_fields, args, &request.args);

    rpc_tx_channel_t *channel = this->SendRequest(&request);
    if (!channel) return -ENOMEM;



    // TODO: Manage the memory that was allocated for the `channel` object
    *(uint8_t *)0 = 0;

    return channel->request->rpc_id;
}

int RPCTarget::PostRequest(const Message *message)
{
    const Request *request = &message->data.request;
    const InOut *args = &request->args;

    DEBUG("Received Request!\n");

    InOut *result = new InOut(); // Free'd by `loop` after the RPCHandler has indicated that it is done
    if (!result)
    {
        DEBUG("Failed to allocate result\n");
        pb_release(Message_fields, (Message *) message);
        delete message;
        return -ENOMEM;
    }
    
    // DEBUG("RPCTarget::PostRequest Result Ptr: %p\n", result);

    RPCHandler *handler = this->_controller->Dispatch(request->func, message, args, result);
    if (handler == NULL)
    {
        DEBUG("Failed to dispatch handler\n");
        pb_release(Message_fields, (Message *) message);
        delete message;
        delete result; // Don't need pb_release, because it was never used
        return -EINVAL;
    }

    // DEBUG("RPCTarget::PostRequest Handler ptr: %p\n", handler);

    // Add to the work queue
    this->_workq.push_back(handler);

    return 0;
}

/**
 * @brief Processes an incoming cancel from a client
 * @param message The message received
 * @return 0 on success, negative on error
 */
int RPCTarget::PostCancel(const Message *message)
{
    // Not implemented yet
    return 0;

    // // Find the handler in the work queue
    // for (auto it = this->_workq.begin(); it != this->_workq.end(); it++)
    // {
    //     RPCHandler *handler = *it;
    //     if (handler->_message->rpc_id == message->rpc_id)
    //     {
    //         int rc = 0;
    //         if ((rc = handler->cancel()) == 0)
    //         {
    //             // Remove from the work queue
    //             this->_workq.erase(it);

    //             // Free the handler
    //             delete handler;

    //             return 0;
    //         }
    //         else
    //         {
    //             return rc;
    //         }
    //     }
    // }

    // return -EINVAL;
}

int RPCTarget::PostResponse(const Message *message)
{
    DEBUG("Received Response!\n");

    // Look up the RPC ID in the wait queue
    for (auto it = this->_waitq.begin(); it != this->_waitq.end(); it++)
    {
        rpc_tx_channel_t *channel = *it;
        if (channel->request->rpc_id == message->rpc_id)
        {
            // Copy the response
            channel->response = (Message*)message;
            channel->flags = 1;

            // Let the caller clean it up

            return 0;
        }
    }

    return -EINVAL;
}

int RPCTarget::Post(const Message *message)
{
    // Validate the message
    // Expect has ID, target ID, etc

    DEBUG("Target received message!\n");

    if (message->target != this->_controller->get_id()) {
        DEBUG("Target ID mismatch!\n");
        return -EINVAL;
    }

    // Make sure the message ID is one greater than the last one we received
    uint32_t message_id = message->m_id;
    bool message_is_first = message_id == 1;
    bool message_is_next = message_id == this->last_recv_message_id + 1;

    this->last_recv_message_id = message_id;
    if (!message_is_first && !message_is_next) {
        DEBUG("Message ID mismatch!\n");
        return -EINVAL;
    }

    // Copy the memory so that we can manage it and let the caller manage the original object
    Message *message_copy = new Message(); // Managed by the cooresponding `Post*` function, which free's in `loop` if successful
    if (!message_copy) return -ENOMEM;
    pb_copy(Message_fields, message, message_copy);

    int rc = 0;
    switch (message->which_data)
    {
        case Message_request_tag:
            rc = this->PostRequest(message_copy);
            break;
        case Message_cancel_tag:
            rc = this->PostCancel(message_copy);
            break;
        case Message_response_tag:
            rc = this->PostResponse(message_copy);
            break;
        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}

int RPCTarget::loop()
{
    this->_io->loop();
    while (true)
    {
        Message message = Message_init_zero;
        int rc = this->_io->Read(&message);
        if (rc == 0)
        {
            rc = this->Post(&message);
        }
        else
        {
            break;
        }
    }

    // Check the work queue
    for (uint8_t i = 0; i < this->_workq.size(); i++)
    {
        RPCHandler *handler = this->_workq[i];
        int rc = handler->loop();
        if (rc > 0)
        {
            continue;
        }
        else
        {
            // bool is_record = handler->_result->which_value == InOut_record_tag;

            // if (is_record)
            // {
            //     DEBUG("RPCTarget::loop\n");
            //     DEBUG("Handler ptr: %p\n", handler);
            //     DEBUG("Result ptr: %p\n", handler->_result);
            //     DEBUG("Record Ptr: %p\n", &handler->_result->value.record);
            //     DEBUG("Record Samples Ptr: %p\n", handler->_result->value.record.samples);
            // }

            // The work item has finished, either sucessfully or with an error
            // We need to send the response back to the client with the status/result
            Message message = Message_init_zero;
            message.version = 1;

            message.has_m_id = true;
            message.m_id = this->last_sent_message_id + 1;
            this->last_sent_message_id = message.m_id;

            message.has_rpc_id = true;
            message.rpc_id = handler->_message->rpc_id;

            message.has_source = true;
            message.source = this->_controller->get_id();

            message.has_target = true;
            message.target = handler->_message->source;

            message.which_data = Message_response_tag;
            Response *response = &message.data.response;
            
            if (rc == 0)
            {
                response->has_type = true;
                response->type = ResponseType_SUCCESS;

                response->has_result = true; 
                
                // TODO: Make this work. I don't think the pb_decode step is allocating the arrays at all
                // pb_copy(InOut_fields, handler->_result, &response->result);
                response->result = *handler->_result;
            }
            else
            {
                response->has_type = true;
                response->type = ResponseType_ERROR;

                response->has_result = true;
                response->result.which_value = InOut_error_tag;
                response->result.value.error.has_code = true;
                response->result.value.error.code = rc;
            }

            int rc = this->_io->Write(&message);
            
            // Remove from the work queue
            this->_workq.erase(this->_workq.begin() + i);
            
            // Free the handler
            pb_release(InOut_fields, handler->_result);
            delete handler->_result;
            handler->_result = NULL;

            // Permissive cast is OK because we add the const qualifier to prevent the handler from modifying it
            pb_release(Message_fields, (Message *) handler->_message); 
            delete handler->_message;
            handler->_message = NULL;

            delete handler;

            if (rc != 0) return rc;
            break;
        }
    }

    // TODO: Update if there is a timeout
    // // Check the wait queue
    // for (auto it = this->_waitq.begin(); it != this->_waitq.end(); it++)
    // {
    //     rpc_tx_channel_t *channel = *it;
    //     if (channel->flags == 1)
    //     {
    //         // Remove from the wait queue
    //         this->_waitq.erase(it);

    //         // Free the channel
    //         delete channel;
    //     }
    // }

    return 0;
}