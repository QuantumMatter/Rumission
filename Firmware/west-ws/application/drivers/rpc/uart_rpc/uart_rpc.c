#define DT_DRV_COMPAT rumission_uart_rpc

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include <lib/rpc/rpc.h>
#include <drivers/rpc/rpc_device.h>
#include "Protos/rpc.pb.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uart_rpc, LOG_LEVEL_ERR);

static void rx_thread_entry(void *, void *, void *);

enum uart_events {
    UART_EVT_RX_RDY = 1,
};

struct uart_rpc_data {
    struct ring_buf *rx_buffer;
    struct ring_buf *tx_buffer;
    struct k_event *uart_events;
};

struct uart_rpc_config {
    const struct device *rpc_device;
    const struct device *uart_dev;
};

static bool uart_rpc_read(struct rpc *rpc, struct rpc_target *target, Message *message)
{
    // This driver will reads from the UART device in a separate thread
    // so we don't need to worry about explicitly reading here
    return false;
}

static bool pb_write_callback(pb_ostream_t *stream, const pb_byte_t *buf, size_t count)
{
    struct device *dev = stream->state;
    const struct uart_rpc_config *config = dev->config;
    struct uart_rpc_data *data = dev->data;

    int len = ring_buf_put(data->tx_buffer, buf, count);
    if (len != 0)
    {
        uart_irq_tx_enable(config->uart_dev);
    }

    return len == count;
}

static bool uart_rpc_write(struct rpc *rpc, struct rpc_target *target, Message *message)
{
    const struct device *dev = target->user_data;

    // Get the size of the message
    pb_ostream_t size_stream = PB_OSTREAM_SIZING;
    if (!pb_encode(&size_stream, Message_fields, message))
    {
        LOG_ERR("Could not get size of message! %s", PB_GET_ERROR(&size_stream));
        return false;
    }

    printk("Sending %d bytes\n", size_stream.bytes_written);

    pb_ostream_t ostream = {
        .callback = pb_write_callback,
        .state = dev,
        .max_size = 1024,
        .bytes_written = 0,
    };

    return pb_encode(&ostream, Message_fields, message);
}

static bool pb_read_callback(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    size_t bytes_read = 0;
    size_t write_index = 0;

    const struct device *dev = stream->state;
    struct uart_rpc_data *data = dev->data;

    while (write_index < count)
    {
        uint32_t available = ring_buf_size_get(data->rx_buffer);
        if (available)
        {
            bytes_read = ring_buf_get(data->rx_buffer, &buf[write_index], MIN(available, (count - write_index)));
            write_index += bytes_read;
        }
        else
        {
            if (stream->bytes_left == SIZE_MAX)
            {
                // Wait forever for the start of the stream
                k_event_wait(data->uart_events, UART_EVT_RX_RDY, true, K_FOREVER);
            }
            else
            {
                // Assume that parts of the stream will be close to each other
                // Expect one part to arrive within 5msec of the next
                // Otherwise assume we're reached the end of the message
                int rc = k_event_wait(data->uart_events, UART_EVT_RX_RDY, true, K_MSEC(5));
                if (rc == 0)
                {
                    stream->bytes_left = 0;
                    return false;
                }
                else
                {
                    // We received a new char before the timeout, so let's process it
                    continue;
                }
            }
        }
    }

    return write_index == count;
}

static void uart_irq_callback(const struct device *uart_dev, void *this_dev)
{
    uint8_t c;

    const struct device *dev = this_dev;
    struct uart_rpc_data *data = dev->data;

    if (!uart_irq_update(uart_dev)) return;

    if (uart_irq_rx_ready(uart_dev))
    {
        // while (uart_fifo_read(uart_dev, &c, 1) == 1)
        // while (uart_irq_rx_ready(uart_dev))
        {
            if (uart_fifo_read(uart_dev, &c, 1) == 1)
            {
                ring_buf_put(data->rx_buffer, &c, 1);

                // Post RX event
                // if (!posix_is_cpu_running())
                // {
                //     nsi_print_trace("Waking up CPU\n");
                //     posix_interrupt_raised();
                // }
                k_event_set(data->uart_events, UART_EVT_RX_RDY);
            }
        }
    }
    
    if (uart_irq_tx_ready(uart_dev))
    {
        uint8_t *bytes = NULL;
        uint32_t len = ring_buf_get_claim(data->tx_buffer, &bytes, 10);
        if (len)
        {
            // printk("Sending: %d bytes: ", len);
            // for (uint32_t i = 0; i < len; i++)
            // {
            //     printk("0x%X, ", bytes[i]);
            // }
            // printk("\n");

            int rc = uart_fifo_fill(uart_dev, bytes, len);
            if (rc != len)
            {
                LOG_ERR("Could not fill UART TX FIFO! %d", rc);
            }
        }
        else
        {
            uart_irq_tx_disable(uart_dev);
        }
        ring_buf_get_finish(data->tx_buffer, len);
    }

}

static int uart_rpc_init(const struct device *dev)
{
    const struct uart_rpc_config *config = dev->config;

    if (!device_is_ready(config->uart_dev))
    {
        LOG_ERR("UART is NOT ready");
        return -ENODEV;
    }

    int rc = uart_irq_callback_user_data_set(config->uart_dev, uart_irq_callback, dev);
    if (rc != 0)
    {
        LOG_ERR("Could not set UART IRQ callback! 0x%X", rc);
        return rc;
    }

    uart_irq_rx_enable(config->uart_dev);

    return 0;
}

static void rx_thread_entry(void *p1, void *p2, void *p3)
{
    const struct device *dev = p1;
    struct uart_rpc_data *data = dev->data;
    struct uart_rpc_config *config = dev->config;

    while (true)
    {
        Message message;
        pb_istream_t istream = {
            .callback = pb_read_callback,
            .state = dev,
            .bytes_left = SIZE_MAX,
        };

        if (pb_decode(&istream, Message_fields, &message))
        {
            rpc_post_dt(config->rpc_device, message.source, &message);
        }
    }
}

static struct rpc_io_api_s uart_rpc_io_api = {
    .read = uart_rpc_read,
    .write = uart_rpc_write,
};

#define UART_RPC_INST_DEFINE(i)                                             \
                                                                            \
    RING_BUF_DECLARE(rpc_rx_buffer_##i, CONFIG_UART_RPC_BUFFER_LENGTH);     \
    RING_BUF_DECLARE(rpc_tx_buffer_##i, CONFIG_UART_RPC_BUFFER_LENGTH);     \
    K_EVENT_DEFINE(rpc_uart_events_##i);                                    \
                                                                            \
    static struct uart_rpc_data uart_rpc_data_##i = {                       \
        .rx_buffer = &rpc_rx_buffer_##i,                                    \
        .tx_buffer = &rpc_tx_buffer_##i,                                    \
        .uart_events = &rpc_uart_events_##i,                                \
    };                                                                      \
                                                                            \
    static const struct uart_rpc_config uart_rpc_config_##i = {             \
        .rpc_device = DEVICE_DT_GET(DT_INST_PHANDLE(i, rpc)),               \
        .uart_dev = DEVICE_DT_GET(DT_INST_BUS(i)),                          \
    };                                                                      \
                                                                            \
    DEVICE_DT_INST_DEFINE(i,                                                \
                          uart_rpc_init,                                    \
                          NULL,                                             \
                          &uart_rpc_data_##i,                               \
                          &uart_rpc_config_##i,                             \
                          POST_KERNEL,                                      \
                          CONFIG_APPLICATION_INIT_PRIORITY,                 \
                          &uart_rpc_io_api);                                \
                                                                            \
    K_THREAD_DEFINE(uart_rpc_rx_tid_##i, CONFIG_UART_RPC_RX_STACK_SIZE,     \
                    rx_thread_entry,                                        \
                    DEVICE_DT_INST_GET(i), NULL, NULL,                      \
                    CONFIG_UART_RPC_RX_PRIORITY, 0, 10);

DT_INST_FOREACH_STATUS_OKAY(UART_RPC_INST_DEFINE)
