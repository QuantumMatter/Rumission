#ifndef __RPC_SOCKET_H__
#define __RPC_SOCKET_H__

#include "RPC.h"

class RPCSocket: public RPCIO
{
    public:
        RPCSocket(uint16_t port);
        ~RPCSocket();

        int loop();
        int Read(Message *dest);
        int Write(Message *src);

    private:
        int _server_fd;
        int _client_fd;

        uint8_t rx_buffer[2048];
        uint8_t rx_buffer_index = 0;

        std::vector<Message*> _rx_messages;
};

#endif // __RPC_SOCKET_H__