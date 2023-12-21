#define DT_DRV_COMPAT ti_lmp91000

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lmp91000, LOG_LEVEL_ERR);

#include <drivers/misc/lmp91000/lmp91000.h>
#include "lmp91000_regs.h"

union reg_bytes {
    struct lmp91000_lock lock;
    struct lmp91000_tiacn tiacn;
    struct lmp91000_refcn refcn;
    struct lmp91000_modecn modecn;
    uint8_t byte;
};

struct lmp91000_data {
    bool locked;
    enum lmp91000_tia_gain tia_gain;
    enum lmp91000_rload rload;
    bool ref_source_ext;
    enum lmp91000_int_z int_z;
    enum lmp91000_bias_sign bias_sign;
    enum lmp91000_bias bias;
    bool fet_shorting;
    enum lmp91000_op_mode op_mode;
};

struct lmp91000_config {
    struct i2c_dt_spec bus;
};

static int lmp91000_init(const struct device *dev)
{
    const struct lmp91000_config *config = dev->config;
    uint8_t value;
    int rc;

    if (!device_is_ready(config->bus.bus)) {
        LOG_ERR("I2C Bus isn't ready yet");
        return -ENODEV;
    }

    rc = i2c_reg_read_byte_dt(&config->bus, 0x00, &value);
    if (rc != 0) {
        LOG_ERR("Could not read from LMP");
        return rc;
    }

    uint8_t status = value & BIT(0);
    if (0 == status) {
        LOG_ERR("LMP not ready yet");
        return -EAGAIN;
    }

    LOG_ERR("LMP91000 INIT OK: 0x%X", config->bus.addr);

    return 0;
}

int lmp91000_lock(const struct device *dev, bool lock)
{
    const struct lmp91000_config *config = dev->config;
    struct lmp91000_data *data = dev->data;
    union reg_bytes reg;
    int rc;

    if (data->locked == lock) {
        return 0;
    }

    reg.byte = 0;
    reg.lock.lock = lock;

    rc = i2c_reg_write_byte_dt(&config->bus, LMP91000_LOCK_REG, reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not write to LMP");
        return rc;
    }

    data->locked = lock;

    return 0;
}

int lmp91000_set_tia_gain(const struct device *dev, enum lmp91000_tia_gain gain)
{
    const struct lmp91000_config *config = dev->config;
    struct lmp91000_data *data = dev->data;
    union reg_bytes reg;
    int rc;

    if (data->locked) {
        LOG_ERR("LMP is locked");
        return -EACCES;
    }

    if (data->tia_gain == gain) {
        return 0;
    }

    rc = i2c_reg_read_byte_dt(&config->bus, LMP91000_TIACN_REG, &reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not read from LMP");
        return rc;
    }

    reg.tiacn.tia_gain = gain;

    rc = i2c_reg_write_byte_dt(&config->bus, LMP91000_TIACN_REG, reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not write to LMP");
        return rc;
    }

    data->tia_gain = gain;

    return 0;
}

int lmp91000_set_rload(const struct device *dev, enum lmp91000_rload rload)
{
    const struct lmp91000_config *config = dev->config;
    struct lmp91000_data *data = dev->data;
    union reg_bytes reg;
    int rc;

    if (data->locked) {
        LOG_ERR("LMP is locked");
        return -EACCES;
    }

    if (data->rload == rload) {
        return 0;
    }

    rc = i2c_reg_read_byte_dt(&config->bus, LMP91000_TIACN_REG, &reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not read from LMP");
        return rc;
    }

    reg.tiacn.rload = rload;

    rc = i2c_reg_write_byte_dt(&config->bus, LMP91000_TIACN_REG, reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not write to LMP");
        return rc;
    }

    data->rload = rload;

    return 0;
}

int lmp91000_set_ref_source(const struct device *dev, bool ref_source_ext)
{
    const struct lmp91000_config *config = dev->config;
    struct lmp91000_data *data = dev->data;
    union reg_bytes reg;
    int rc;

    if (data->locked) {
        LOG_ERR("LMP is locked");
        return -EACCES;
    }

    if (data->ref_source_ext == ref_source_ext) {
        return 0;
    }

    rc = i2c_reg_read_byte_dt(&config->bus, LMP91000_REFCN_REG, &reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not read from LMP");
        return rc;
    }

    reg.refcn.ref_source = ref_source_ext;

    rc = i2c_reg_write_byte_dt(&config->bus, LMP91000_REFCN_REG, reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not write to LMP");
        return rc;
    }

    data->ref_source_ext = ref_source_ext;

    return 0;
}

int lmp91000_set_bias_sign(const struct device *dev, enum lmp91000_bias_sign bias_sign)
{
    const struct lmp91000_config *config = dev->config;
    struct lmp91000_data *data = dev->data;
    union reg_bytes reg;
    int rc;

    if (data->locked) {
        LOG_ERR("LMP is locked");
        return -EACCES;
    }

    if (data->bias_sign == bias_sign) {
        return 0;
    }

    rc = i2c_reg_read_byte_dt(&config->bus, LMP91000_REFCN_REG, &reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not read from LMP");
        return rc;
    }

    reg.refcn.bias_sign = bias_sign;

    rc = i2c_reg_write_byte_dt(&config->bus, LMP91000_REFCN_REG, reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not write to LMP");
        return rc;
    }

    data->bias_sign = bias_sign;

    return 0;
}

int lmp91000_set_bias(const struct device *dev, enum lmp91000_bias bias)
{
    const struct lmp91000_config *config = dev->config;
    struct lmp91000_data *data = dev->data;
    union reg_bytes reg;
    int rc;

    if (data->locked) {
        LOG_ERR("LMP is locked");
        return -EACCES;
    }

    if (data->bias == bias) {
        return 0;
    }

    rc = i2c_reg_read_byte_dt(&config->bus, LMP91000_REFCN_REG, &reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not read from LMP");
        return rc;
    }

    reg.refcn.bias = bias;

    rc = i2c_reg_write_byte_dt(&config->bus, LMP91000_REFCN_REG, reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not write to LMP");
        return rc;
    }

    data->bias = bias;

    return 0;
}

int lmp91000_set_op_mode(const struct device *dev, enum lmp91000_op_mode op_mode)
{
    const struct lmp91000_config *config = dev->config;
    struct lmp91000_data *data = dev->data;
    union reg_bytes reg;
    int rc;

    if (data->locked) {
        LOG_ERR("LMP is locked");
        return -EACCES;
    }

    if (data->op_mode == op_mode) {
        return 0;
    }

    rc = i2c_reg_read_byte_dt(&config->bus, LMP91000_MODECN_REG, &reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not read from LMP");
        return rc;
    }

    reg.modecn.op_mode = op_mode;

    rc = i2c_reg_write_byte_dt(&config->bus, LMP91000_MODECN_REG, reg.byte);
    if (rc != 0) {
        LOG_ERR("Could not write to LMP");
        return rc;
    }

    data->op_mode = op_mode;

    return 0;
}


#define LMP91000_INST_DEFINE(i)                                     \
    static struct lmp91000_data lmp91000_data_##i = {               \
        .locked = true,                                             \
        .tia_gain = LMP91000_TIA_GAIN_EXT,                          \
        .rload = LMP91000_RLOAD_100OHM,                             \
        .ref_source_ext = false,                                    \
        .int_z = INT_Z_50,                                          \
        .bias_sign = LMP91000_BIAS_SIGN_NEG,                        \
        .bias = BIAS_0,                                             \
        .fet_shorting = false,                                      \
        .op_mode = LMP91000_OP_MODE_DEEP_SLEEP,                     \
    };                                                              \
                                                                    \
                                                                    \
    static const struct lmp91000_config lmp91000_config_##i = {     \
        .bus = I2C_DT_SPEC_INST_GET(i),                             \
    };                                                              \
                                                                    \
    DEVICE_DT_INST_DEFINE(i, lmp91000_init, NULL,                   \
                            &lmp91000_data_##i,                     \
                            &lmp91000_config_##i,                   \
                            POST_KERNEL,                            \
                            CONFIG_ADC_INIT_PRIORITY,            \
                            NULL);

DT_INST_FOREACH_STATUS_OKAY(LMP91000_INST_DEFINE)