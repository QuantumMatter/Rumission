#define DT_DRV_COMPAT axetris_axetris

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(axetris, LOG_LEVEL_ERR);

#define AXETRIS_MSG_SIZE    (128)
#define AXETRIS_Q_SIZE      (8)

#define AXETRIS_MONITOR_STACK_SIZE  (1024)
#define AXETRIS_MONITOR_PRIORITY    (7)

struct axetris_msgq_item {
    struct device *dev;
    uint8_t data[AXETRIS_MSG_SIZE];
};

K_MSGQ_DEFINE(axetris_msgq, AXETRIS_MSG_SIZE + 8, AXETRIS_Q_SIZE, 4);

void axetris_monitor_entry(void *, void *, void *);
K_THREAD_DEFINE(axetris_tid, AXETRIS_MONITOR_STACK_SIZE,
                axetris_monitor_entry, NULL, NULL, NULL,
                AXETRIS_MONITOR_PRIORITY, 0, 10);

union bytes_float {
    float f;
    uint8_t b[4];
};

enum axetris_cmd_id {
    AXETRIS_CMD_PING = 'P',
    AXETRIS_CMD_CONFIG = 'C',
    AXETRIS_CMD_START = 'M',
    AXETRIS_CMD_IDLE = 'I',
    AXETRIS_CMD_SAVE = 'S',
    AXETRIS_CMD_VERSION = 'V',
    AXETRIS_CMD_STATUS = 'D',
};

struct axetris_config {
    const struct device *uart_dev;
    const struct gpio_dt_spec rs232_ok;
};

struct axetris_data {
    bool rx_in_frame;
    uint8_t rx_buf[AXETRIS_MSG_SIZE];
    uint8_t rx_pos;

    struct sensor_value methane;
};

static void uart_irq_callback(const struct device *uart_dev, void *axetris_dev)
{
    uint8_t c;

    if (!uart_irq_update(uart_dev)) {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev)) {
        return;
    }

    while (uart_fifo_read(uart_dev, &c, 1) == 1) {
        // LOG_ERR("UART RX: 0x%X", c);
        

        const struct device *dev = axetris_dev;
        struct axetris_data *data = dev->data;

        // State machine based on whether we are in a frame or not
        if (!data->rx_in_frame)
        {
            if (c == '{') {
                // ASSERT(data->rx_pos == 0);
                // LOG_ERR("UART RX: START FRAME");
                data->rx_in_frame = true;
                data->rx_buf[data->rx_pos++] = c;
            }
        }
        else
        {
            if (!data->rx_in_frame) {
                // LOG_ERR("UART RX: NOT IN FRAME");
                continue;
            }

            if (data->rx_pos >= AXETRIS_MSG_SIZE) {
                LOG_ERR("RX buffer overflow");
                continue;
            }

            data->rx_buf[data->rx_pos++] = c;

            if (c == '}') {
                data->rx_in_frame = false;

                struct axetris_msgq_item item = {
                    .dev = dev,
                    .data = { 0 },
                };

                memcpy(item.data, data->rx_buf, sizeof(data->rx_buf));

                int rc = k_msgq_put(&axetris_msgq, &item, K_NO_WAIT);
                if (rc != 0) {
                    LOG_ERR("Could not put message in queue: %d", rc);
                }
                data->rx_pos = 0;
                memset(data->rx_buf, 0, sizeof(data->rx_buf));
                // LOG_ERR("UART RX: END FRAME");
            }
        }
    }
}

static int axetris_init(const struct device *dev)
{
    const struct axetris_config *config = dev->config;

    printk("Axetris UART Device: %p\n", config->uart_dev);

    if (!device_is_ready(config->uart_dev)) {
        LOG_ERR("UART Bus isn't ready yet");
        return -ENODEV;
    }

    int rc = uart_irq_callback_user_data_set(config->uart_dev, uart_irq_callback, dev);
    if (rc != 0) {
        LOG_ERR("Could not set UART IRQ callback");
        return rc;
    }
    
	uart_irq_rx_enable(config->uart_dev);

    if (!device_is_ready(config->rs232_ok.port)) {
        LOG_ERR("RS232 OK GPIO isn't ready yet");
        return -ENODEV;
    }
    // rc = gpio_pin_configure_dt(&config->rs232_ok, GPIO_INPUT);
    // if (rc != 0) {
    //     LOG_ERR("Could not configure RS232 OK GPIO: %d", rc);
    //     return rc;
    // }

    int rs232_ok = gpio_pin_get_dt(&config->rs232_ok);
    LOG_ERR("RS232 OK: %d", rs232_ok);

    return 0;
}

static int axetris_sample_fetch(const struct device *dev, enum sensor_channel channel)
{
    return 0;
}

void axetris_monitor_entry(void *p1, void *p2, void *p3)
{
    LOG_ERR("AXETRIS MONITOR STARTED");

    // uint8_t bytes[AXETRIS_MSG_SIZE];
    struct axetris_msgq_item item;

    while (k_msgq_get(&axetris_msgq, &item, K_FOREVER) == 0)
    {
        uint8_t *bytes = item.data;
        const struct device *dev = item.dev;
        struct axetris_data *data = dev->data;

        if (bytes[0] != '{') {
            LOG_ERR("Missing Axetris Frame Start");
            goto end;
        }

        enum axetris_cmd_id cmd_id = bytes[1];
        uint16_t cmd_len = bytes[2] | (bytes[3] << 8);

        if (bytes[cmd_len - 1] != '}') {
            LOG_ERR("Missing Axetris Frame End");
            goto end;
        }

        uint8_t checksum = 0;
        for (uint16_t i = 0; i < cmd_len - 3; i++)
        {
            checksum -= bytes[i];
        }
        if (checksum != bytes[cmd_len - 2]) {
            LOG_ERR("Invalid Axetris Frame Checksum");
            goto end;
        }

        // printk("Received Axetris Frame: \n\t");
        // for (uint16_t i = 0; i < cmd_len; i++)
        // {
        //     printk("0x%X ", bytes[i]);
        // }
        // printk("\n");

        if (cmd_id == AXETRIS_CMD_START)
        {
            uint16_t error_code = bytes[4] | (bytes[5] << 8);
            union bytes_float conv;

            conv.b[0] = bytes[6];
            conv.b[1] = bytes[7];
            conv.b[2] = bytes[8];
            conv.b[3] = bytes[9];

            float conc = conv.f;
            // printk("\tMeasurement:\n");
            // printk("\t\tError Code: 0x%X\n", error_code);
            // printk("\t\tConcentration: %f\n", conc);

            data->methane.val1 = (int32_t) conc;
            data->methane.val2 = (int32_t) ((conc - data->methane.val1) * 1000000);
        }

        end:
            memset(bytes, 0, AXETRIS_MSG_SIZE);
            continue;
    }
}

static int axetris_sample_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{

    if (chan != 60) {
        return -ENOTSUP;
    }

    struct axetris_data *data = dev->data;
    memcpy(val, &data->methane, sizeof(struct sensor_value));

    return 0;
}

static const struct sensor_driver_api axetris_api = {
    .sample_fetch = axetris_sample_fetch,
    .channel_get = axetris_sample_get,
};

#define AXETRIS_INST_DEFINE(i)                                      \
                                                                    \
    static struct axetris_data axetris_data_##i = {           \
        .rx_in_frame = false,                                       \
        .rx_buf = { 0 },                                            \
        .rx_pos = 0,                                                \
    };                                                              \
                                                                    \
    static const struct axetris_config axetris_config_##i = {       \
        .uart_dev = DEVICE_DT_GET(DT_INST_BUS(i)),                  \
        .rs232_ok = GPIO_DT_SPEC_INST_GET(i, input_gpios),            \
    };                                                              \
                                                                    \
    SENSOR_DEVICE_DT_INST_DEFINE(i, axetris_init, NULL,             \
                                    &axetris_data_##i,              \
                                    &axetris_config_##i,            \
                                    POST_KERNEL,                    \
                                    CONFIG_SENSOR_INIT_PRIORITY,    \
                                    &axetris_api);                  \

DT_INST_FOREACH_STATUS_OKAY(AXETRIS_INST_DEFINE)
