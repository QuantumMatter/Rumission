#include <zephyr/device.h>

// Attach telnet to the port defined in the devicetree

int main(void)
{
    const struct device *uart_socket = DEVICE_DT_GET(DT_NODELABEL(uart_socket));
    if (device_is_ready(uart_socket))
    {
        printk("UART socket is ready\n");
    }
    else
    {
        printk("UART socket is not ready\n");
    }

    return 0;
}