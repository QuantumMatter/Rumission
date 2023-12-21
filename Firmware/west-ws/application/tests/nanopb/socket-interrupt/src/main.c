#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include <Protos/sample.pb.h>
#include <Protos/rpc.pb.h>

enum uart_evt {
    UART_EVT_RX_RDY = 1,
};

static struct device *uart_socket = DEVICE_DT_GET(DT_NODELABEL(uart_socket));

K_EVENT_DEFINE(uart_events);
RING_BUF_DECLARE(tx_buffer, 128);
RING_BUF_DECLARE(rx_buffer, 128);

static bool pb_read_callback(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    ssize_t bytes_read = 0;
    ssize_t write_index = 0;

    printk("Reading %zd bytes\n", count);

    while (write_index < count)
    {
        printk("%d < %d\n", write_index, count);

        uint32_t length = ring_buf_size_get(&rx_buffer);
        printk("%d bytes available in RX buffer\n", length);

        if (length)
        {
            bytes_read = ring_buf_get(&rx_buffer, &buf[write_index], MIN(length, (count - write_index)));
            printk("Pulled %d bytes from RX buffer\n", bytes_read);
            write_index += bytes_read;
        }
        else
        {
            if (stream->bytes_left == SIZE_MAX)
            {
                printk("Waiting for start of message\n");
                // Wait forever for the start of the stream
                k_event_wait(&uart_events, UART_EVT_RX_RDY, true, K_FOREVER);
            }
            else
            {
                // Assume that parts of the stream will be close to each other
                // Expect one part to arrive within 5msec of the next
                // Otherwise assume we're reached the end of the message
                int rc = k_event_wait(&uart_events, UART_EVT_RX_RDY, true, K_MSEC(5));
                if (rc == 0)
                {
                    stream->bytes_left = 0;
                    return false;
                }
                else
                {
                    // pass
                }
            }
        }
    }

    if (write_index == count)
    {
        return true;
    }

    if (bytes_read < 0) {
        printk("Error reading: %s\n", strerror(errno));
        return false;
    }

    if (bytes_read == 0)
    {
        printk("Connection closed\n");
        return false;
    }

    return false;
}

void uart_irq_callback(void *user_data)
{
    printk("UART IRQ callback\n");

    uint8_t c;

    if (!uart_irq_update(uart_socket)) {
        printk("UART IRQ update failed\n");
        return;
    }

    while (uart_irq_rx_ready(uart_socket))
    {
        if (uart_fifo_read(uart_socket, &c, 1) == 1) {
            printk("Received: 0x%X\n", c);
            ring_buf_put(&rx_buffer, &c, 1);

            // Post RX event
            k_event_set(&uart_events, UART_EVT_RX_RDY);
        }
    }

    if (uart_irq_tx_ready(uart_socket))
    {
        uint8_t *data = NULL;
        uint32_t len = ring_buf_get_claim(&tx_buffer, &data, 10);
        if (len) {
            printk("Sending: %.*s\n", len, data);
            int rc = uart_fifo_fill(uart_socket, data, len);
            if (rc < 0)
            {
                printk("Error sending: %d\n", rc);
            }
            printk("Sent %d bytes\n", rc);
        }
        else
        {
            printk("TX buffer empty\n");
            uart_irq_tx_disable(uart_socket);
        }
        ring_buf_get_finish(&tx_buffer, len);
    }
    else
    {
        printk("UART IRQ TX not ready\n");
    }
}

int main(void)
{
    uart_irq_callback_user_data_set(uart_socket, uart_irq_callback, NULL);
    uart_irq_rx_enable(uart_socket);

    while (1)
    {
        Message message = Message_init_zero;
        pb_istream_t istream = {
            .callback = pb_read_callback,
            .state = NULL,
            .bytes_left = SIZE_MAX,
        };

        printk("Waiting for Message...\n");

        bool pb_ok = pb_decode(&istream, Message_fields, &message);

        if (pb_ok)
        {
            printk("Decoded message\n");

            printk("Message ID: ");
            if (message.has_m_id)    printk("%d\n", message.m_id);
                                else printk("N/A\n");

            printk("RPC ID: ");
            if (message.has_rpc_id)  printk("%d\n", message.rpc_id);
                                else printk("N/A\n");

            printk("Target: ");
            if (message.has_target)  printk("%d\n", message.target);
                                else printk("N/A\n");


            printk("Which Data: %d\n", message.which_data);
            if (message.which_data == Message_request_tag)
            {
                Request *request = &message.data.request;
                printk("Request:\n");

                printk("  Function: ");
                if (request->has_func)   printk("%d\n", request->func);
                                    else printk("N/A\n");
            }
        }
        else
        {
            printk("Error decoding message: %s\n", PB_GET_ERROR(&istream));
        }
    }
    

    return 0;
}