#define DT_DRV_COMPAT rumission_rpc

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <drivers/rpc/rpc_device.h>
#include <lib/rpc/rpc.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rpc_device, LOG_LEVEL_ERR);

struct rpc_data {};
struct rpc_config {
    struct rpc *rpc;
    const struct device **children;
    const uint8_t child_count;
};

int rpc_register_target_dt(const struct device *dev, int target_id, const struct device *io_device)
{
    const struct rpc_config *config = dev->config;
    int target_idx = rpc_register_target(config->rpc, target_id, io_device->api);
    if (target_idx < 0)
    {
        return target_idx;
    }

    struct rpc_target *target = &config->rpc->targets[target_idx];
    target->user_data = io_device;

    LOG_ERR("Registering target %d (%d) with HW device %s @ %p", target_id, target_idx, io_device->name, io_device);

    return 0;
}

inline int rpc_call_dt(const struct device *dev, int target_id, FUNCTION func_id, const InOut *arg, InOut *result, k_timeout_t timeout)
{

    const struct rpc_config *config = dev->config;

    // LOG_ERR("rpc_call_dt(%s: %p, target: %d, func: %d, arg: %p, result: %p, timeout: %d)", dev->name, dev, target_id, func_id, arg, result, timeout.ticks);

    return rpc_call(config->rpc, target_id, func_id, arg, result, timeout);
}

inline int rpc_post_dt(const struct device *dev, uint32_t target_id, Message *message)
{
    const struct rpc_config *config = dev->config;
    return rpc_post(config->rpc, target_id, message);
}

static int rpc_init(const struct device *dev)
{
    const struct rpc_config *config = dev->config;

    LOG_ERR("rpc_init(%s: %p); bus_id = %d", dev->name, dev, config->rpc->id);

    // printk("RPC Children: ");
    // for (uint8_t i = 0; i < config->child_count; i++)
    // {
    //     printk("%s ", config->children[i]->name);
    // }
    // printk("\n");

    return 0;
}

// #define RPC_GET_CHILD_DEV_PTS(node_id)  DEVICE_DT_GET(node_id)
// DT_FOREACH_CHILD_STATUS_OKAY(DT_INST(i, DT_DRV_COMPAT), DEVICE_DT_GET)

#define RPC_INST_DEFINE(i)                                          \
                                                                    \
    static struct rpc_data rpc_data_##i;                            \
                                                                    \
    static const struct device *rpc_children_##i[] = {              \
        \
    };                                                              \
                                                                    \
    static struct rpc rpc##i = {                                    \
        .id = DT_INST_PROP(i, bus_id),                              \
        .targets = { 0 },                                           \
        .rpc_target_count = 0,                                      \
        .functions = { 0 },                                         \
        .function_count = 0,                                        \
    };                                                              \
                                                                    \
    static const struct rpc_config rpc_config_##i = {               \
        .rpc = &rpc##i,                                             \
        .children = rpc_children_##i,                               \
        .child_count = ARRAY_SIZE(rpc_children_##i),                \
    };                                                              \
                                                                    \
    DEVICE_DT_INST_DEFINE(i,                                        \
                          rpc_init,                                 \
                          NULL,                                     \
                          &rpc_data_##i,                            \
                          &rpc_config_##i,                          \
                          POST_KERNEL,                              \
                          CONFIG_SENSOR_INIT_PRIORITY,              \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(RPC_INST_DEFINE)
