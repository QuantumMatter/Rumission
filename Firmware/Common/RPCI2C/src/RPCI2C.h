#ifndef __RPC_I2C_H__
#define __RPC_I2C_H__

#include "RPC.h"

#include <Arduino.h>
#include <Wire.h>

enum RPCI2CRegister
{
    RPC_I2C_REG_LENGTH = 0,
    RPC_I2C_REG_MESSAGE = 1,
    RPC_I2C_REG_MAX = 2,
};

class RPCI2C : public RPCIO
{
    public:
        RPCI2C(TwoWire *wire);
        ~RPCI2C();

        int loop();
        int Read(Message *dest);
        int Write(Message *src);

        void onReceive(int numBytes);
        void onRequest();

        void enableDebug(Stream *port) { this->_debug_port = port; };

    private:
        TwoWire *_wire;

        RPCI2CRegister _reg = RPC_I2C_REG_LENGTH;

        uint64_t last_rx;
        uint8_t rx_buffer[2048];
        uint8_t rx_buffer_index = 0;

        uint8_t tx_buffer[2048];
        uint8_t tx_buffer_index = 0;
        uint8_t tx_buffer_length = 0;

        std::vector<Message*> _tx_messages;
        std::vector<Message*> _rx_messages;

        Stream *_debug_port = NULL;

};

#endif // __RPC_I2C_H__