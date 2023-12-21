#ifndef __RPC_H__
#define __RPC_H__

#include <zephyr/kernel.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "Protos/sample.pb.h"
#include "Protos/rpc.pb.h"

// Forward declare the RPC struct
struct rpc;
struct rpc_target;

struct rpc_rx_channel {
    struct k_work work;
    Message *message;
    struct rpc_target *client;
    struct rpc *rpc;
};

struct rpc_tx_channel {
    struct k_event events;
    Message *message;
    Message *response;
};

struct rpc_target;

typedef bool (*rpc_read_t)(struct rpc *arg, struct rpc_target *target, Message *message);
typedef bool (*rpc_write_t)(struct rpc *arg, struct rpc_target *target, Message *message);

struct rpc_io_api_s {
    rpc_read_t read;
    rpc_write_t write;
};

/**
 * @brief RPC target
 * @details Keeps track of the RPC target's state
*/
struct rpc_target {
    uint32_t target_id;
    uint32_t last_recv_message_id; ///!< `m_id` of the most recently received message
    uint32_t last_sent_message_id; ///!< `m_id` of the most recently sent message
    uint32_t last_sent_rpc_id; ///!< `rpc_id` of the most recently sent message
    struct rpc_rx_channel rx_channels[CONFIG_RPC_CONCURRENT_REQUESTS];
    uint32_t rx_channels_mask; ///!< Bitmask of active received channels
    struct rpc_tx_channel tx_channels[CONFIG_RPC_CONCURRENT_REQUESTS];
    uint32_t tx_channels_mask; ///!< Bitmask of active sent channels

    void *user_data;

    const struct rpc_io_api_s *io_api;
};

typedef uint32_t (rpc_function_t)(const InOut *arg, InOut *result);

struct rpc_function {
    int function_id;
    rpc_function_t *function;
    uint32_t result_type;
};

struct rpc {
    uint32_t id;

    struct rpc_target targets[CONFIG_RPC_TARGET_COUNT];
    int rpc_target_count;

    struct rpc_function functions[CONFIG_RPC_FUNCTION_COUNT];
    int function_count;
};

int rpc_register_function(struct rpc *rpc, int function_id, rpc_function_t function, uint32_t result_type);
int rpc_register_target(struct rpc *rpc, int target_id, const struct rpc_io_api_s *io_api);

/**
 * @brief Posts a message that has been received from a target to the controller
 * @param rpc RPC instance
 * @param client_id ID of the client that sent the message
 * @param message Message to process. This is copied, so it can be managed by the caller
 * @return 0 on success, negative error code on failure
 */
int rpc_post(struct rpc *rpc, uint32_t client_id, Message *message);

int rpc_post_result(struct rpc *rpc, int target, int channel, void *result);

int rpc_call(struct rpc *rpc, uint32_t target_id, uint32_t function, const InOut *args, InOut *result, k_timeout_t timeout);

void message_print(Message *message, uint8_t level);


#endif // __RPC_H__