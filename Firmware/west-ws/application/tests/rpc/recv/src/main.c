#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <lib/rpc/rpc.h>
#include <Protos/v1/rpc.h>
#include <drivers/sensor/flow.h>

#include <errno.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(logging_main);


const static struct device *flow = DEVICE_DT_GET(DT_NODELABEL(flow));
const static struct device *pam = DEVICE_DT_GET(DT_NODELABEL(pam));


int main(void)
{
    const struct device *devices[] = {
        // uart_socket,
        flow,
        pam,
    };

    for (uint8_t i = 0; i < ARRAY_SIZE(devices); i++)
    {
        if (!device_is_ready(devices[i]))
        {
            printk("Device %s is NOT ready\n", devices[i]->name);
            return -ENODEV;
        }
    }

    while (true)
    {
        k_sleep(K_MSEC(5000));

        printf("Pinging Flow Meter...\n");
        flow_meter_ping(flow, K_MSEC(100));
    }

    return 0;
}
