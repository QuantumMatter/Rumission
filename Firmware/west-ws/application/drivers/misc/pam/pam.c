#define DT_DRV_COMPAT twobtech_pam

#include <drivers/misc/pam/pam.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <drivers/rpc/rpc_device.h>
#include <lib/rpc/rpc.h>
#include <Protos/v1/rpc.h>

#include "Protos/sample.pb.h"
#include "Protos/rpc.pb.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pam, LOG_LEVEL_ERR);

struct pam_config {    
    const uint8_t bus_id;
    const struct device *rpc_device;
    const struct device *io_device;
};

struct pam_data {};

int pam_ping(const struct device *dev, k_timeout_t timeout)
{
    const struct pam_config *config = dev->config;

    InOut args = { 0 };
    args.which_value = InOut_integer_tag;
    args.value.integer = 0;

    InOut result = { 0 };

    int rc = rpc_call_dt(config->rpc_device, config->bus_id, FUNC_PING, &args, &result, timeout);
    if (rc != 0)
    {
        LOG_ERR("Could not call PING on remote PAM: %d\n", rc);
        return rc;
    }

    bool success = result.value.integer == (args.value.integer + 1);

    return success ? 0 : -EINVAL;
}

int pam_post_record(const struct device *dev, Record *record, k_timeout_t timeout)
{
    const struct pam_config *config = dev->config;

    InOut args = { 0 };
    args.which_value = InOut_record_tag;
    if (!pb_copy(Record_fields, record, &args.value.record))
    {
        LOG_ERR("Could not copy record to args");
        pb_release(InOut_fields, &args);
        return -EINVAL;
    }

    InOut result = { 0 };
    int rc = rpc_call_dt(config->rpc_device, config->bus_id, FUNC_POST_RECORD, &args, &result, timeout);
    if (rc != 0)
    {
        LOG_ERR("Could not call POST_RECORD on remote PAM: %d", rc);
        pb_release(InOut_fields, &args);
        pb_release(InOut_fields, &result);
        return rc;
    }
    
    pb_release(InOut_fields, &args);
    pb_release(InOut_fields, &result);

    return 0;
}

static int pam_init(const struct device *dev)
{
    const struct pam_config *config = dev->config;

    int target_idx = rpc_register_target_dt(config->rpc_device, config->bus_id, config->io_device);
    LOG_ERR("PAM RPC Target IDX: %d", target_idx);
    
    return 0;
}

#define PAM_INST_DEFINE(i)                                                                  \
                                                                                            \
    static struct pam_config pam_config_##i = {                                             \
        .bus_id = DT_REG_ADDR(DT_INST(i, DT_DRV_COMPAT)),                                   \
        .rpc_device = DEVICE_DT_GET(DT_INST_PARENT(i)),                                     \
        .io_device = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_INST(i, DT_DRV_COMPAT), hw, 0)),    \
    };                                                                                      \
                                                                                            \
    static struct pam_data pam_data_##i = {};                                               \
                                                                                            \
    DEVICE_DT_INST_DEFINE(i, pam_init, NULL,                                                \
                            &pam_data_##i,                                                  \
                            &pam_config_##i,                                                \
                            POST_KERNEL,                                                    \
                            CONFIG_APPLICATION_INIT_PRIORITY,                               \
                            NULL);

DT_INST_FOREACH_STATUS_OKAY(PAM_INST_DEFINE)
