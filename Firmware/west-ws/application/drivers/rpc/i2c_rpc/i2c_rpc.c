#define DT_DRV_COMPAT rumission_i2c_rpc

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#include <drivers/rpc/rpc_device.h>
#include <lib/rpc/rpc.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(i2c_rpc, LOG_LEVEL_ERR);

struct i2c_rpc_data {
    uint8_t rx_buffer[2048];
    uint16_t rx_index;
};

struct i2c_rpc_config {
    const struct i2c_dt_spec i2c_dt;
    const struct device *rpc;
};

static bool i2c_rpc_write(struct rpc *rpc, struct rpc_target *target, Message *message)
{
    const struct device *io_device = target->user_data;
    const struct i2c_rpc_config *config = io_device->config;

    // Get the wire size of the message
    pb_ostream_t size_stream = { 0 };
    pb_encode(&size_stream, Message_fields, message);
    size_t message_size = size_stream.bytes_written;

    uint8_t *buffer = k_malloc(message_size); // Free'd before return; can't be known at compile time
    if (buffer == NULL) {
        LOG_ERR("Could not allocate buffer for message!");
        return false;
    }

    // Encode the message into the buffer
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, message_size);
    bool success = pb_encode(&stream, Message_fields, message);
    if (!success)
    {
        k_free(buffer);
        return false;
    }

    // // Print the message for debugging
    // printk("Sending message to I2C device 0x%X : %s (%d bytes)\n", config->i2c_dt.addr, config->i2c_dt.bus->name, message_size);
    // for (uint8_t i = 0; i < message_size; i++)
    // {
    //     printk("0x%X ", buffer[i]);
    // }
    // printk("\n");

    // Write the message to the I2C bus
    int rc = i2c_write_dt(&config->i2c_dt, buffer, message_size);
    k_free(buffer);
    
    if (rc != 0)
    {
        LOG_ERR("Could not write message to I2C bus! %d", rc);
        return false;
    }

    return true;
}

static bool i2c_rpc_read(struct rpc *rpc, struct rpc_target *target, Message *message)
{

    return false;

    // const struct i2c_rpc_config *config = target->user_data;
    // // const struct device *i2c_dev = config->i2c_dev.bus;

    // // Read single bytes from the I2C device until we receive an error
    // // Assume that this means that the device has no more data to send
    // // and we have the entire message

    // // TODO: A lot of devices support a different type of API, where
    // // if the target sends the NACK early (indiciating end of message)
    // // the number of bytes that were read and an error are returned
    // // We should see if zephyr supports this API instead because it
    // // would be incredibly more efficient

    // uint8_t alloc_count = 1;
    // uint8_t write_idx = 0;
    // uint8_t *buffer = k_malloc(128);

    // while (true)
    // {
    //     int rc = i2c_read_dt(&config->i2c_dt, &buffer[write_idx++], 1);
    //     if (rc != 0)
    //     {
    //         break;
    //     }

    //     // If we're at the end of the message buffer, allocate more space
    //     if (write_idx == (128 * alloc_count))
    //     {
    //         alloc_count++;
    //         buffer = realloc(buffer, alloc_count);
    //     }
    // }

    // // Decode the message
    // pb_istream_t stream = pb_istream_from_buffer(buffer, write_idx);
    // bool success = pb_decode(&stream, Message_fields, message);
    // k_free(buffer);

    // return success;
}

static int i2c_rpc_init(const struct device *dev)
{
    const struct i2c_rpc_config *config = dev->config;
    const struct device *rpc_device = config->rpc;

    LOG_ERR("Setting up RPC Target on I2C device 0x%X : %s", config->i2c_dt.addr, config->i2c_dt.bus->name);

    return 0;
}

static void i2c_rx_thread_entry(void *p1, void *p2, void *p3)
{
    uint64_t loop_start, loop_duration;
    int rc;

    const struct device *i2c_rpc = p1;
    const struct i2c_rpc_config *config = i2c_rpc->config;
    struct i2c_rpc_data *data = i2c_rpc->data;

    uint8_t *message_buffer = NULL;
    Message message = Message_init_zero;
    // Message *message = NULL;

    while (true)
    {
        loop_start = k_uptime_get();

        // Read from reg 0 to get the length of the message in its buffer
        const uint8_t length_reg = 0;
        rc = i2c_write_dt(&config->i2c_dt, &length_reg, 1);
        if (rc != 0) { LOG_ERR("Could not set channel!"); goto loop_end; }

        uint8_t length_buffer[2] = { 0, };
        rc = i2c_read_dt(&config->i2c_dt, length_buffer, 2);
        if (rc != 0) { LOG_ERR("Could not read message length from I2C device! 0x%X", rc); goto loop_end; }

        uint16_t message_length = (length_buffer[0] << 8) | length_buffer[1];
        
        if (message_length == 0)
        {
            // LOG_ERR("No message available");
            goto loop_end;
        }
        else
        {
            LOG_ERR("Message length: %d", message_length);
        }

        // Read the message from the I2C device
        const uint8_t message_reg = 1;
        rc = i2c_write_dt(&config->i2c_dt, &message_reg, 1);
        if (rc != 0) { LOG_ERR("Could not set channel!"); goto loop_end; }

        message_buffer = k_malloc(message_length); // Free'd before loop continues; can't be known at compile time
        if (message_buffer == NULL) { LOG_ERR("Could not allocate buffer for message!"); goto loop_end; }
        rc = i2c_read_dt(&config->i2c_dt, message_buffer, message_length);
        if (rc != 0) { LOG_ERR("Could not read message from I2C device! 0x%X", rc); goto loop_end; }

        printk("Received data from device (%d bytes): \n", message_length);
        for (uint16_t i = 0; i < message_length; i++)
        {
            printk("0x%0X, ", message_buffer[i]);

            if ((i % 10) == 9)
                printk("\n");
        }
        printk("\n");

        // message = k_malloc(sizeof(Message));
        // if (message == NULL) { LOG_ERR("Could not allocate message! (%d bytes)", sizeof(Message)); goto loop_end; }
        // memset(message, 0, sizeof(Message));

        // printk("Message: %p\n", message);
        pb_istream_t stream = pb_istream_from_buffer(message_buffer, message_length);
        bool success = pb_decode(&stream, Message_fields, &message);

        if (success)
        {
            // LOG_ERR("Received full Message!\n");

            rpc_post_dt(config->rpc, message.source, &message);
        }
        else
        {
            LOG_ERR("Could not decode message!");
            LOG_ERR("Error: %s", PB_GET_ERROR(&stream));
        }
        
        loop_end:
        {
            if (message_buffer != NULL)
            {
                k_free(message_buffer);
                message_buffer = NULL;
            }

            if (success)
            {
                pb_release(Message_fields, &message);
            }
            memset(&message, 0, sizeof(Message));

            loop_duration = k_uptime_get() - loop_start;
            if (loop_duration < CONFIG_I2C_RPC_RX_PERIOD)
            {
                // printk("%lld, %lld, %d ms\n", loop_duration, CONFIG_I2C_RPC_RX_PERIOD - loop_duration, CONFIG_I2C_RPC_RX_PERIOD);
                k_sleep(K_MSEC(CONFIG_I2C_RPC_RX_PERIOD - loop_duration));
            }
        }
    }
}

static struct rpc_io_api_s i2c_rpc_io_api = {
    .read = i2c_rpc_read,
    .write = i2c_rpc_write,
};

#define I2C_RPC_INST_DEFINE(i)                                      \
                                                                    \
    K_THREAD_DEFINE(i2c_rpc_rx_thread,                              \
                    CONFIG_I2C_RPC_RX_STACK_SIZE,                   \
                    i2c_rx_thread_entry,                            \
                    DEVICE_DT_INST_GET(i), NULL, NULL,              \
                    CONFIG_I2C_RPC_RX_PRIORITY,                     \
                    0,                                              \
                    10);                                            \
                                                                    \
    static const struct i2c_rpc_config i2c_rpc_config_##i = {       \
        .i2c_dt = I2C_DT_SPEC_INST_GET(i),                          \
        .rpc = DEVICE_DT_GET(DT_INST_PHANDLE(i, rpc)),              \
    };                                                              \
                                                                    \
    static struct i2c_rpc_data i2c_rpc_data_##i = {                 \
        .rx_index = 0,                                              \
        .rx_buffer = { 0 },                                         \
    };                                                              \
                                                                    \
    DEVICE_DT_INST_DEFINE(i,                                        \
                          i2c_rpc_init,                             \
                          NULL,                                     \
                          &i2c_rpc_data_##i,                        \
                          &i2c_rpc_config_##i,                      \
                          POST_KERNEL,                              \
                          CONFIG_APPLICATION_INIT_PRIORITY,         \
                          &i2c_rpc_io_api);

DT_INST_FOREACH_STATUS_OKAY(I2C_RPC_INST_DEFINE)