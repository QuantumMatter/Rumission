#define DT_DRV_COMPAT rumission_flow_meter

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include <drivers/sensor/flow.h>
#include <drivers/rpc/rpc_device.h>

#include <Protos/rpc.pb.h>
#include <lib/rpc/rpc.h>
#include <Protos/v1/rpc.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(flow_meter_device, LOG_LEVEL_ERR);

struct flow_meter_data {};

struct flow_meter_config {
    const uint8_t bus_id;
    const struct device *rpc_device;
    const struct device *io_device;
};

int flow_meter_ping(const struct device *dev, k_timeout_t timeout)
{
    const struct flow_meter_config *config = dev->config;

    InOut args = { 0 };
    args.which_value = InOut_integer_tag;
    args.value.integer = 0;

    InOut result = { 0 };

    int rc = rpc_call_dt(config->rpc_device, config->bus_id, FUNC_PING, &args, &result, timeout);
    if (rc != 0)
    {
        LOG_ERR("Could not call PING on remote flow meter: %d\n", rc);
        return rc;
    }

    bool success = result.value.integer == (args.value.integer + 1);

    return success ? 0 : -EINVAL;
}

int flow_meter_fetch_record(const struct device *dev, Record *record, k_timeout_t timeout)
{
    const struct flow_meter_config *config = dev->config;

    LOG_ERR("Fetching record from flow meter %d", config->bus_id);

    InOut args = { 0 };
    args.which_value = InOut_integer_tag;
    args.value.integer = 0;

    InOut result = { 0 };
    int rc = rpc_call_dt(config->rpc_device, config->bus_id, FUNC_GET_RECORD, &args, &result, timeout);

    LOG_ERR("Fetch Record returned: %d", rc);

    if (rc == 0)
    {
        // Validate the response
        bool valid = true;
        valid &= result.which_value == InOut_record_tag;

        if (!valid)
        {
            LOG_ERR("Received invalid response from RPC call!");
            rc = -EINVAL;
        }
        else
        {
            if (!pb_copy(Record_fields, &result.value.record, record))
            {
                LOG_ERR("Could not copy record from RPC response!");
                rc = -EINVAL;
            }
        }
    }

    pb_release(InOut_fields, &args);
    pb_release(InOut_fields, &result);

    return rc;
}

static int flow_meter_sample_fetch(const struct device *dev, enum sensor_channel channel)
{
    const struct flow_meter_config *config = dev->config;

    return 0;
}

static int flow_meter_sample_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    const struct flow_meter_config *config = dev->config;

    return 0;
}

static bool validate(const uint8_t bus_id, const struct device *rpc_device, const struct device *io_device)
{

    LOG_ERR("Flow Meter ID: %d", bus_id);
    LOG_ERR("RPC Device: %s", rpc_device->name);
    LOG_ERR("IO Device: %s", io_device->name);

    return false;
}

static int flow_meter_init(const struct device *dev)
{
    const struct flow_meter_config *config = dev->config;

    validate(config->bus_id, config->rpc_device, config->io_device);

    // LOG_ERR("Flow Meter (%d) is using %s as its HW IO device", config->bus_id, config->io_device->name);

    int target_idx = rpc_register_target_dt(config->rpc_device, config->bus_id, config->io_device);
    LOG_ERR("Flow Meter RPC Target IDX: %d", target_idx);

    return 0;
}

static const struct sensor_driver_api flow_meter_driver_api = {
    .sample_fetch = flow_meter_sample_fetch,
    .channel_get = flow_meter_sample_get,
};

#define FLOW_METER_INST_DEFINE(i)                                                           \
                                                                                            \
    static struct flow_meter_data flow_meter_data_##i;                                      \
                                                                                            \
    static const struct flow_meter_config flow_meter_config_##i = {                         \
        .bus_id = DT_REG_ADDR(DT_INST(i, DT_DRV_COMPAT)),                                   \
        .rpc_device = DEVICE_DT_GET(DT_INST_PARENT(i)),                                     \
        .io_device = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_DRV_INST(i), hw, 0)),             \
    };                                                                                      \
                                                                                            \
    SENSOR_DEVICE_DT_INST_DEFINE(i,                                                         \
                                 flow_meter_init,                                           \
                                 NULL,                                                      \
                                 &flow_meter_data_##i,                                      \
                                 &flow_meter_config_##i,                                    \
                                 POST_KERNEL,                                               \
                                 CONFIG_SENSOR_INIT_PRIORITY,                               \
                                 &flow_meter_driver_api);

DT_INST_FOREACH_STATUS_OKAY(FLOW_METER_INST_DEFINE)
