#ifndef __RPC_DEVICE_H__
#define __RPC_DEVICE_H__

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <Protos/rpc.pb.h>

int rpc_register_target_dt(const struct device *dev, int target_id, const struct device *io_device);
int rpc_call_dt(const struct device *dev, int target_id, FUNCTION func_id, const InOut *arg, InOut *result, k_timeout_t timeout);

/**
 * @brief Helper function for `rpc_post`
 * @param dev The rumission,rpc DTS device
 * @param target_id ID of the client that sent the message
 * @param message Message to process. This is copied by `rpc_post`, so it can be managed by the caller
 */
int rpc_post_dt(const struct device *dev, uint32_t target_id, Message *message);

#endif // __RPC_DEVICE_H__