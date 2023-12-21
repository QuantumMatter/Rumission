#ifndef __RPC_CONTROLLER_H__
#define __RPC_CONTROLLER_H__

#include <Arduino.h>

#include <stdint.h>
#include <vector>

#include <pb.h> // Tells the dependency manager that NanoPB is a dependency, so it generates the files first

#include "rpc.pb.h"
#include "sample.pb.h"

// Foward Declarations
class RPCIO;
class RPCHandler;
class RPCTarget;
class RPCController;

class RPCIO {
    public:
        RPCIO() {};
        ~RPCIO() {};
        
        virtual int loop() = 0;
        virtual int Read(Message *dest) = 0;

        /**
         * @brief API For writing a message to the transport (actually puts the message on the wire)
         * @param src The message to write. The contents are copied, so the caller is responsible for freeing the data
         * @return int 0 on success, -1 on failure
         */
        virtual int Write(Message *src) = 0;
};

/// @brief A class that handles responds to RPC requests
class RPCHandler {
    public:
        RPCHandler(const Message *message, const InOut *arg, InOut *result): _message(message), _arg(arg), _result(result) {};
        ~RPCHandler() {};

    public:
        const Message *_message;
        
        virtual int loop() = 0;
        virtual int cancel() = 0;
        
        const InOut *_arg;
        InOut *_result;
};

typedef struct rpc_tx_channel_s
{
    const Message *request;
    Message *response;

    uint32_t flags;
    uint32_t timeout_ms;
} rpc_tx_channel_t;

typedef union message_data_u
{
    Request request;
    Response response;
    Cancel cancel;
} message_data_t;

class RPCTarget {
    public:
        RPCTarget(RPCController *controller, uint32_t id, RPCIO* io);
        ~RPCTarget();
        
        int Call(FUNCTION function_id, const InOut *args, InOut *result, uint32_t timeout_ms);

        /**
         * @brief Called by a target when they have received a message
         * @param message The message received. This is copied so that the original can be managed by the caller
         * @return 0 on success, negative on error
         */
        int Post(const Message *message);

        int loop();

        void enableDebug(Stream *port) { this->_debug_port = port; };

    private:
        /**
         * @brief Processes an incoming request from a client
         * @param message The message received
         * @return 0 on success, negative on error
         */
        int PostRequest(const Message *message);
        int PostCancel(const Message *message);
        int PostResponse(const Message *message);

        /**
         * @brief Helper method for sending a request (just calls `SendMessage`)
         * @param request The request to send. This is copied by `SendMessage`, so the caller is responsible for freeing the data
         * @return rpc_tx_channel_t* A pointer to the channel that will be used to track the message
         */
        rpc_tx_channel_t* SendRequest(const Request *request);


        rpc_tx_channel_t* SendResponse(const Response *response);

        /**
         * @brief Sends a message to the target
         * @note This method handles the state that tracks the message ID, RPC ID, etc
         * @param tag Indicates the type of message to send (Request, Cancel, Response)
         * @param data The data to include in the message. The contents are copied, so the caller is responsible for freeing the data
         * @return rpc_tx_channel_t* A pointer to the channel that will be used to track the message
         */
        rpc_tx_channel_t* SendMessage(int tag, const message_data_t *data);

        RPCController *_controller;
        RPCIO *_io;

        const uint32_t _id;
        uint32_t last_recv_message_id = 0;
        uint32_t last_sent_message_id = 0;
        uint32_t last_sent_rpc_id = 0;

        std::vector<RPCHandler*> _workq;
        std::vector<rpc_tx_channel_t*> _waitq;

        Stream *_debug_port = NULL;

};

class RPCController {
    public:
        RPCController(uint32_t id);
        ~RPCController();

        RPCTarget *RegisterTarget(uint32_t target_id, RPCIO *io);

        // To be implemented in main.cpp
        static RPCHandler *Dispatch(FUNCTION function_id, const Message *message, const InOut *arg, InOut *result);

        const uint32_t get_id() { return this->_id; }

        int loop();

    private:
        uint32_t _id;
        std::vector<RPCTarget*> _targets;
};

bool pb_copy(const pb_msgdesc_t *fields, const void *src, void *dest);

#endif // __RPC_CONTROLLER_H__