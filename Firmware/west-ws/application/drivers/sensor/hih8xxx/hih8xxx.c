#define DT_DRV_COMPAT honeywell_hih8xxx

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hih8xxx, LOG_LEVEL_ERR);

struct hih8xxx_data {
    struct sensor_value humidity;
    struct sensor_value temperature;
};

struct hih8xxx_config {
    const struct i2c_dt_spec i2c_dev;
};

static int hih8xxx_init(const struct device *dev)
{
    const struct hih8xxx_config *config = dev->config;

    // if (!device_is_ready(config->i2c_dev.bus)) {
    //     LOG_ERR("I2C Bus isn't ready yet");
    //     return -ENODEV;
    // }

    return 0;
}

static int hih8xxx_sample_fetch(const struct device *dev, enum sensor_channel channel)
{
    const struct hih8xxx_config *config = dev->config;

    return i2c_write_dt(&config->i2c_dev, NULL, 0);
}


// TODO: Make this be triggered by a timer after the app calls `hih8xxx_sample_fetch`
// The datasheet says that a conversion typically takes 37 msec
// Not important for this rough version though
static int hih8xxx_sample_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    const struct hih8xxx_config *config = dev->config;
    struct hih8xxx_data *data = dev->data;

    bool chan_is_humidity = (chan == SENSOR_CHAN_HUMIDITY);
    bool chan_is_temp = (chan == SENSOR_CHAN_AMBIENT_TEMP);

    if (!(chan_is_humidity || chan_is_temp)) {
        LOG_ERR("Invalid channel requested!");
        return -ENOTSUP;
    }

    uint8_t recv[4] = { 0 };
    int rc = i2c_read_dt(&config->i2c_dev, recv, ARRAY_SIZE(recv));
    if (rc != 0) {
        LOG_ERR("Could not retreive sample from HIH! 0x%X", rc);
        return rc;
    }

    uint8_t status = (( recv[0] & 0b11000000 ) >> 6) & 0b11;
    if (status == 0b00) {
        // pass - normal operation
    } else if (status == 0b01) {
        LOG_ERR("HIH retreived stale data!");
        // but not a critical problem
    } else if (status == 0b10) {
        LOG_ERR("HIH in command mode!");
        return -ENODEV;
    } else if (status == 0b11) {
        LOG_ERR("HIH has unknown status!");
        return -ENODEV;
    } else {
        k_panic();
    }

    uint16_t humidity_data = (( recv[0] & 0b00111111 ) << 8) | recv[1];
    uint16_t temperature_data = ( recv[2] << 6) | (( recv[3] & 0b11111100 ) >> 2);

    float rh = (float)humidity_data / 16382.0 * 100.0;
    float temp_c = (float)temperature_data / 16382.0 * 165.0 - 40.0;

    data->humidity.val1 = (int32_t)rh;
    data->humidity.val2 = (rh - data->humidity.val1) * 1000000;

    data->temperature.val1 = (int32_t)temp_c;
    data->temperature.val2 = (temp_c - data->temperature.val1) * 1000000;

    if (chan_is_humidity) {
        memcpy(val, &data->humidity, sizeof(struct sensor_value));
    } else if (chan_is_temp) {
        memcpy(val, &data->temperature, sizeof(struct sensor_value));
    } else {
        k_panic();
    }

    return 0;
}

static const struct sensor_driver_api hih8xxx_api = {
    .sample_fetch = hih8xxx_sample_fetch,
    .channel_get = hih8xxx_sample_get,
};

#define HIH8XXX_INST_DEFINE(i)                                      \
                                                                    \
    static struct hih8xxx_data hih8xxx_data_##i = {                 \
        .humidity = {0},                                            \
        .temperature = {0},                                         \
    };                                                             \
                                                                    \
    static struct hih8xxx_config hih8xxx_config_##i = {             \
        .i2c_dev = I2C_DT_SPEC_INST_GET(i),                         \
    };                                                              \
                                                                    \
    SENSOR_DEVICE_DT_INST_DEFINE(i, hih8xxx_init, NULL,             \
                                    &hih8xxx_data_##i,              \
                                    &hih8xxx_config_##i,            \
                                    POST_KERNEL,                    \
                                    CONFIG_SENSOR_INIT_PRIORITY,    \
                                    &hih8xxx_api);                  \

DT_INST_FOREACH_STATUS_OKAY(HIH8XXX_INST_DEFINE)
