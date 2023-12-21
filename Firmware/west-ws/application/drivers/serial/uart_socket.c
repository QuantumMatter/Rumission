#define DT_DRV_COMPAT rumission_uart_socket

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <errno.h>
#include "posix_native_task.h"

#include <nsi_tracing.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uart_socket, LOG_LEVEL_ERR);

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <fcntl.h>


#include <zephyr/irq.h>
#include "posix_soc.h"
#include "irq_ctrl.h"
#include "board_soc.h"

#define UART_SOCKET_MONITOR_STACK_SIZE  (2048)
#define UART_SOCKET_MONITOR_PRIORITY    (7)

struct uart_socket_config {
    int port;

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
    uint8_t irqn;
#endif // CONFIG_UART_INTERRUPT_DRIVEN
};

struct uart_socket_data {
    int server_fd;
    int client_fd;

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
    volatile bool tx_enabled;
    bool rx_enabled;
    pthread_mutex_t rx_mutex;

    uart_irq_callback_user_data_t callback;
    void *cb_data;

    struct k_timer tx_trampoline;

    pthread_t monitor_thread;
#endif // CONFIG_UART_INTERRUPT_DRIVEN
};

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static int fifo_fill(const struct device *dev, const uint8_t *tx_data, int len);
static int fifo_read(const struct device *dev, uint8_t *rx_data, const int size);
static void irq_tx_enable(const struct device *dev);
static void irq_tx_disable(const struct device *dev);
static int irq_tx_ready(const struct device *dev);
static void irq_rx_enable(const struct device *dev);
static void irq_rx_disable(const struct device *dev);
static int irq_tx_complete(const struct device *dev);
static int irq_rx_ready(const struct device *dev);
// static void irq_err_enable(const struct device *dev);
// static void irq_err_disable(const struct device *dev);
// static int irq_is_pending(const struct device *dev);
static int irq_update(const struct device *dev);
static void irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb, void *user_data);
static void tx_trampoline(struct k_timer *timer);

void uart_socket_isr(const void *arg);

struct timer_container {
    struct k_timer timer;
    const struct device *dev;
};
#endif // CONFIG_UART_INTERRUPT_DRIVEN

static int poll_in(const struct device *dev, unsigned char *p_char);
static void poll_out(const struct device *dev, unsigned char out_char);

static struct uart_driver_api uart_socket_api = {
    
#ifdef CONFIG_UART_INTERRUPT_DRIVEN

    .fifo_fill = fifo_fill,
    .fifo_read = fifo_read,
    .irq_tx_enable = irq_tx_enable,
    .irq_tx_disable = irq_tx_disable,
    .irq_tx_ready = irq_tx_ready,
    .irq_rx_enable = irq_rx_enable,
    .irq_rx_disable = irq_rx_disable,
    .irq_tx_complete = irq_tx_complete,
    .irq_rx_ready = irq_rx_ready,
    // .irq_err_enable = irq_err_enable,
    // .irq_err_disable = irq_err_disable,
    // .irq_is_pending = irq_is_pending,
    .irq_update = irq_update,
    .irq_callback_set = irq_callback_set,

#endif // CONFIG_UART_INTERRUPT_DRIVEN

    .poll_in = poll_in,
    .poll_out = poll_out,
};

static bool try_client(const struct device *dev)
{
    struct uart_socket_data *data = dev->data;

    if (data->client_fd >= 0)
    {
        return true;
    }

    // Attempt to accept a client connection without blocking
    int client_fd = accept4(data->server_fd, NULL, NULL, SOCK_NONBLOCK);
    if (client_fd > 0)
    {
        data->client_fd = client_fd;
        nsi_print_trace("%s accepted client connection\n", dev->name);
        return true;
    }
    else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // nsi_print_trace("%s no client ready\n", dev->name);
            return false;
        }
        else
        {
            nsi_print_trace("Error accepting client connection: %d: %s\n", errno, strerror(errno));
            return false;
        }
    }
}

// Returns 0 if a character has arrived
// Returns -1 if no character is available
static int poll_in(const struct device *dev, unsigned char *p_char)
{
    struct uart_socket_data *data = dev->data;
    if (!try_client(dev)) return -1; // -ENODEV;

    int client_fd = data->client_fd;
    int rc = read(client_fd, p_char, 1);

    if (rc == 1)
    {
        // nsi_print_trace("%s read %c\n", dev->name, *p_char);
        return 0;
    }
    else if (rc == 0)
    {
        nsi_print_trace("%s client closed connection\n", dev->name);
        data->client_fd = -1;
        return -1; // -ENODEV;
    }
    else if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        // No data is available
        return -1;
    }
    else if (rc < 0)
    {
        nsi_print_trace("Error reading from client: %s\n", strerror(errno));
        return rc;
    }
    else
    {
        k_panic();
        return -1;
    }
}

static void poll_out(const struct device *dev, unsigned char out_char)
{
    struct uart_socket_data *data = dev->data;
    if (!try_client(dev)) return;

    int client_fd = data->client_fd;
    int rc = write(client_fd, &out_char, 1);

    if (rc == 1)
    {
        // nsi_print_trace("%s wrote %c\n", dev->name, out_char);
    }
    else if (rc < 0)
    {
        nsi_print_trace("Error writing to client: %s\n", strerror(errno));
    }
}


#ifdef CONFIG_UART_INTERRUPT_DRIVEN

// Expected to be called from the UART ISR, if uart_irq_tx_ready() returns true
// Returns number of bytes written
// Once the user has data available
// They first enable TX interrupt
// Then, call this function from the ISR with said data
// Disable the TX interrupt when they've written all data
static int fifo_fill(const struct device *dev, const uint8_t *tx_data, int len)
{
    struct uart_socket_data *data = dev->data;
    int client_fd = data->client_fd;

    // Quick check to make sure we're connected
    if (data->client_fd < 0) return len; // -ENODEV;

    int rc = write(client_fd, tx_data, len);
    if (rc < 0)
    {
        nsi_print_trace("Error writing to client: %s\n", strerror(errno));
    }

    // nsi_print_trace("Triggering TX trampoline: %p\n", &data->tx_trampoline);

    // Need to call the callback again to see if we can write more data
    // However, can't do this recursively, otherwise application can't
    // acknowledge that progress has been made (advance ring buffer, etc)

    // k_timer_init(&data->tx_trampoline, tx_trampoline, NULL);
    struct timer_container *container = calloc(1, sizeof(struct timer_container));
    k_timer_init(&container->timer, tx_trampoline, NULL);
    container->dev = dev;
    k_timer_user_data_set(&container->timer, container);
    k_timer_start(&container->timer, K_MSEC(0), K_NO_WAIT);

    // return rc;
    return len;
}

// Expected to be called from UART ISR, if uart_irq_rx_ready() return true
// Returns number of bytes read
static int fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
    struct uart_socket_data *data = dev->data;
    int client_fd = data->client_fd;

    // Quick check to make sure we're connected
    if (data->client_fd < 0) return 0; // -ENODEV;

    pthread_mutex_lock(&data->rx_mutex);

    int rc = read(client_fd, rx_data, size);
    if (rc < 0)
    {
        nsi_print_trace("Error reading from client: %s\n", strerror(errno));
        pthread_mutex_unlock(&data->rx_mutex);
        return rc;
    }
    else if (rc == 0)
    {
        // The client has closed the connection,
        // but that shouldn't be handled here
        k_panic();
        return 0;
    }
    else
    {
        // Successfully read some bytes
        pthread_mutex_unlock(&data->rx_mutex);
        return rc;
    }

}

static void irq_tx_enable(const struct device *dev)
{
    struct uart_socket_data *data = dev->data;
    data->tx_enabled = true;

    // The socket will always be ready for new data,
    // so just call the callback immediately
    if (data->callback)
    {
        // while(data->tx_enabled)
        {
            data->callback(dev, data->cb_data);
        }
    }
}

static void irq_tx_disable(const struct device *dev)
{
    struct uart_socket_data *data = dev->data;
    data->tx_enabled = false;
}

static int irq_tx_ready(const struct device *dev)
{
    return true;
}

// Check if UART TX block finished transmission
// When this condition is true, UART device (or whole system) can be power off
static int irq_tx_complete(const struct device *dev)
{
    // Writing to the socket is actually a blocking call
    // so yes, all TX will be done by this point
    return true;
}

// Enable RX interrupt.
static void irq_rx_enable(const struct device *dev)
{
    // We don't have any true interrupts, but we should
    // only notify the user of an event if they ask for it
    // so we'll poll our device for this flag when an event
    // occurs

    struct uart_socket_data *data = dev->data;
    data->rx_enabled = true;
}

// Disable RX interrupt
static void irq_rx_disable(const struct device *dev)
{
    struct uart_socket_data *data = dev->data;
    data->rx_enabled = false;
}

// Check if UART RX buffer has a received char
// TODO: Make sure we're in the "ISR" context
static int irq_rx_ready(const struct device *dev)
{

    // Poll the socket to see if there is data available
    struct uart_socket_data *data = dev->data;
    int client_fd = data->client_fd;

    if (client_fd < 0) return 0; // -ENODEV;

    pthread_mutex_lock(&data->rx_mutex);

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 0,
    };

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(client_fd, &read_fds);

    int activity = select(client_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (activity < 0)
    {
        nsi_print_trace("Error polling client: %s\n", strerror(errno));
        pthread_mutex_unlock(&data->rx_mutex);
        return 0;
    }
    else if (activity == 0)
    {
        // No data available
        pthread_mutex_unlock(&data->rx_mutex);
        return 0;
    }
    else if (FD_ISSET(client_fd, &read_fds))
    {
        // We can perform a non-blocking read
        int bytes_available = 0;
        if (ioctl(client_fd, FIONREAD, &bytes_available) < 0)
        {
            nsi_print_trace("Error getting bytes available: %s\n", strerror(errno));
            pthread_mutex_unlock(&data->rx_mutex);
            return 0;
        }

        pthread_mutex_unlock(&data->rx_mutex);
        return bytes_available > 0;
    }

    // Not sure why we would get here
    k_panic();
    
    return 0;
}

static int irq_update(const struct device *dev)
{
    // no-op
    return 1;
}

static void irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb, void *user_data)
{
    struct uart_socket_data *data = dev->data;
    data->callback = cb;
    data->cb_data = user_data;
}

static void tx_trampoline(struct k_timer *timer)
{
    struct timer_container *container = k_timer_user_data_get(timer);
    const struct device *dev = container->dev;
    struct uart_socket_data *data = dev->data;

    // nsi_print_trace("TX trampoline\n");

    if (data->callback)
    {
        data->callback(dev, data->cb_data);
    }
    else
    {
        nsi_print_trace("TX trampoline callback is NULL\n");
    }

    free(container);
}

void *uart_socket_monitor_thread(void *dev)
{
    const struct device *uart_dev = dev;
    const struct uart_socket_config *config = uart_dev->config;
    struct uart_socket_data *data = uart_dev->data;

    // Leave the mutex unlocked while we're waiting for data
    // Then lock it while the application is consuming the data
    // Can think about it as "who owns the socket"
    // When there is no data, we own the socket
    // Where there is data, the application owns the socket
    // pthread_mutex_unlock(&data->rx_mutex);

    while (true)
    {
        nsi_print_trace("Waiting for client connection\n");

        // Accept a new connection from the server
        int client_fd = accept(data->server_fd, NULL, NULL);
        if (errno == EINVAL)
        {
            nsi_print_trace("Server socket was closed!\n");
            break;
        }
        else if (client_fd < 0)
        {
            nsi_print_trace("Error accepting client connection: %d: %s\n", errno, strerror(errno));
            continue;
        }
        data->client_fd = client_fd;
        nsi_print_trace("Accepted client connection\n");

        if (data->tx_enabled)
        {
            if (data->callback)
            {
                data->callback(uart_dev, data->cb_data);
            }
        }

        bool client_connected = true;
        while (client_connected)
        {
            pthread_mutex_lock(&data->rx_mutex);

            // Wait until we can read from the socket
            struct timeval timeout = {
                .tv_sec = 0,
                .tv_usec = 0,
            };

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(client_fd, &read_fds);

            // The socket is always write-able, so `select`
            // will always return immediately saying there's
            // activity in the `write_fds`

            // fd_set write_fds;
            // FD_ZERO(&write_fds);
            // FD_SET(client_fd, &write_fds);

            int activity = select(client_fd + 1, &read_fds, NULL, NULL, &timeout);
            if (activity < 0)
            {
                nsi_print_trace("Error polling client: %s\n", strerror(errno));
                pthread_mutex_unlock(&data->rx_mutex);
                continue;
            }
            else if (activity == 0)
            {
                // // Check status of the timer
                // if (k_timer_status_get(&data->tx_trampoline) > 0)
                // {
                //     nsi_print_trace("TX trampoline has expired\n");
                // }
                // else if (k_timer_remaining_get(&data->tx_trampoline) == 0)
                // {
                //     nsi_print_trace("TX trampoline has been cancelled\n");
                // }
                // else
                // {
                //     nsi_print_trace("TX trampoline is still active\n");
                // }

                // No data available
                // nsi_print_trace("No activity on socket in 10 seconds\n");
                pthread_mutex_unlock(&data->rx_mutex);
                continue;
            }

            bool should_notify_app = false;
            if (FD_ISSET(client_fd, &read_fds))
            {
                usleep(100);

                // Data available
                // nsi_print_trace("Data available on socket\n");

                // We can perform a non-blocking read on the socket
                // We should see if the client has disconnected before
                // sending an interrupt to the application
                int bytes_available = 0;
                if (ioctl(client_fd, FIONREAD, &bytes_available) < 0)
                {
                    nsi_print_trace("Error getting bytes available: %s\n", strerror(errno));
                    pthread_mutex_unlock(&data->rx_mutex);
                    continue;
                }

                if (bytes_available == 0)
                {
                    // The client has disconnected
                    nsi_print_trace("Client disconnected\n");
                    client_connected = false;

                    // Read the socket so the system can clean it up
                    char c;
                    (void)!read(data->client_fd, &c, 1);
                    close(data->client_fd);

                    data->client_fd = -1;
                }
                else
                {
                    // nsi_print_trace("Data available on socket: %d bytes\n", bytes_available);
                    should_notify_app = true;
                }
            }

            pthread_mutex_unlock(&data->rx_mutex);

            // if (FD_ISSET(client_fd, &write_fds))
            // {
            //     nsi_print_trace("Socket is ready to write\n");
            //     if (data->tx_enabled)
            //     {
            //         should_notify_app = true;
            //     }
            // }

            if (should_notify_app)
            {
                hw_irq_ctrl_set_irq(config->irqn);
                // if (!posix_is_cpu_running()) posix_sw_set_pending_IRQ(3);
                // if (data->callback)
                // {
                //     // if (!posix_is_cpu_running())
                //     // {
                //     //     // nsi_print_trace("Waking up CPU\n");
                //     //     // // posix_interrupt_raised();
                //     //     // posix_sw_set_pending_IRQ(3);
                //     // }
                //     // else
                //     // {
                //         // data->callback(uart_dev, data->cb_data);
                //     // }

                //     // if (posix_is_cpu_running())
                //     // {
                //     //     posix_sw_set_pending_IRQ(3);
                //     // }
                //     // else
                //     // {
                //     //     hw_irq_ctrl_set_irq(3);
                //     // }
                //     data->callback(uart_dev, data->cb_data);
                // }
            }
        }
    }

    close(data->server_fd);
    data->server_fd = -1;

    return NULL;
}

#endif // CONFIG_UART_INTERRUPT_DRIVEN


static int uart_socket_init(const struct device *dev)
{
    const struct uart_socket_config *config = dev->config;
    struct uart_socket_data *data = dev->data;

    data->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (data->server_fd < 0)
    {
        nsi_print_trace("Error opening socket: %s\n", strerror(errno));
        return -1;
    }

    if (!IS_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN))
    {
        nsi_print_trace("Making the socket NON-BLOCKING\n");

        int flags = fcntl(data->server_fd, F_GETFL);
        if (flags < 0)
        {
            nsi_print_trace("Error getting socket flags: %s\n", strerror(errno));
            return -1;
        }

        if (fcntl(data->server_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            nsi_print_trace("Error setting socket flags: %s\n", strerror(errno));
            return -1;
        }
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(config->port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };

    if (bind(data->server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        nsi_print_trace("Error binding socket: %s\n", strerror(errno));
        return -1;
    }

    if (listen(data->server_fd, 1) != 0)
    {
        nsi_print_trace("Error listening on socket: %s\n", strerror(errno));
        return -1;
    }

    nsi_print_trace("%s listening on port %d\n", dev->name, config->port);

    #ifdef CONFIG_UART_INTERRUPT_DRIVEN
    {
        IRQ_CONNECT(config->irqn, 1, uart_socket_isr, dev, 0);
        irq_enable(config->irqn);

        pthread_mutex_init(&data->rx_mutex, NULL);

        // Need to create a native pthread
        // native_posix only allows one k_thread to run at a time
        // so if we're waiting for accept or select to return,
        // we're actually blocking the rest of the program
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, UART_SOCKET_MONITOR_STACK_SIZE);

        int rc = pthread_create(&data->monitor_thread, &attr, uart_socket_monitor_thread, (void *)dev);
        if (rc != 0)
        {
            nsi_print_trace("Error creating monitor thread: %s\n", strerror(rc));
            return -1;
        }
    }
    #endif // CONFIG_UART_INTERRUPT_DRIVEN

    return 0;
}

#define UART_SOCKET_INIT(i)                                         \
                                                                    \
    static struct uart_socket_data uart_socket_data_##i = {         \
        .server_fd = -1,                                            \
        .client_fd = -1,                                            \
    };                                                              \
                                                                    \
    static struct uart_socket_config uart_socket_config_##i = {     \
        .port = DT_INST_PROP(i, reg),                              \
        .irqn = DT_INST_PROP(i, irqn),                              \
    };                                                              \
                                                                    \
    DEVICE_DT_INST_DEFINE(i, uart_socket_init, NULL,                \
                          &uart_socket_data_##i,                    \
                          &uart_socket_config_##i,                  \
                          PRE_KERNEL_1,                             \
                          CONFIG_SERIAL_INIT_PRIORITY,              \
                          &uart_socket_api);

DT_INST_FOREACH_STATUS_OKAY(UART_SOCKET_INIT);


void uart_socket_isr(const void *arg)
{

    const struct device *dev = arg;
    const struct uart_socket_config *config = dev->config;
    struct uart_socket_data *data = dev->data;

    if (posix_is_cpu_running())
    {
        posix_sw_clear_pending_IRQ(config->irqn);
    }
    else
    {
        hw_irq_ctrl_clear_irq(config->irqn);
    }

    if (data->callback)
    {
        data->callback(dev, data->cb_data);
    }

    printk("UART SOCKET ISR\n");
    return 0;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

void cleanup_socket(struct uart_socket_data *data)
{
    nsi_print_trace("Cleaning up socket\n");
    if (data->client_fd >= 0)
    {
        shutdown(data->client_fd, SHUT_RDWR);
    }
    shutdown(data->server_fd, SHUT_RDWR);
    pthread_join(data->monitor_thread, NULL);
    pthread_mutex_destroy(&data->rx_mutex);
    // pthread_cancel(data->monitor_thread);
}

#define UART_SOCKET_UNINIT(i)                           \
                                                        \
    static void uart_socket_uninit_##i(void)            \
    {                                                   \
        cleanup_socket(&uart_socket_data_##i);          \
    }                                                   \
                                                        \
    NATIVE_TASK(uart_socket_uninit_##i, ON_EXIT, 1);    

DT_INST_FOREACH_STATUS_OKAY(UART_SOCKET_UNINIT);

#endif // CONFIG_UART_INTERRUPT_DRIVEN