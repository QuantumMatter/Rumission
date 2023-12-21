#include "RPCI2C.h"

#include <pb_encode.h>
#include <pb_decode.h>

#include "USB.h"

RPCI2C::~RPCI2C() {}
RPCI2C::RPCI2C(TwoWire *wire) : _wire(wire) {}

#define DEBUG(fmt, ...) if (this->_debug_port != NULL) this->_debug_port->printf(fmt, ##__VA_ARGS__)

void nanopb_debug(char *msg)
{
    // USBSerial.printf("%s\n", msg);
}

void RPCI2C::onReceive(int numBytes) 
{
    DEBUG("onReceive: %d\n", numBytes);

    // Controller will send one byte (reg addr) to select the "channel"
    // that it wants to read from (message length or the message itself)
    // TODO: make a class the encompasses the register/channel access
    // and simply switch between them when receiving a matching byte

    if (numBytes == 1)
    {
        uint8_t reg = this->_wire->read();
        DEBUG("onReceive: reg = %02x\n", reg);

        if (reg >= RPC_I2C_REG_MAX)
        {
            DEBUG("onReceive: invalid register\n");
            return;
        }

        this->_reg = (RPCI2CRegister) reg;
    }
    else
    {
        DEBUG("onReceive: reading message\n");
        for (int i = 0; i < numBytes; i++)
        {
            this->rx_buffer[this->rx_buffer_index++] = this->_wire->read();
            DEBUG("rx[%02d] = %02x\n", this->rx_buffer_index - 1, this->rx_buffer[this->rx_buffer_index - 1]);

            this->last_rx = millis();
        }

        Message *message = new Message(); // Managed by the consumer of the `_rx_messages` vector (RPCI2C::Read, which copies and frees)
        pb_istream_t istream = pb_istream_from_buffer(this->rx_buffer, this->rx_buffer_index);
        if (pb_decode(&istream, Message_fields, message))
        {
            DEBUG("Message received!\n");
            this->rx_buffer_index = 0;
            this->_rx_messages.push_back(message);

            DEBUG("%d messages pending to be read\n", this->_rx_messages.size());
        }
        else
        {
            free(message);
        }
    }
}

void RPCI2C::onRequest()
{

    if (this->_reg == RPC_I2C_REG_LENGTH)
    {
        DEBUG("onRequest: sending length\n");
        this->_wire->write(0);
        this->_wire->write(this->tx_buffer_length);
    }
    else if (this->_reg == RPC_I2C_REG_MESSAGE)
    {
        DEBUG("onRequest: sending message\n");
        this->_wire->write(this->tx_buffer, this->tx_buffer_length);

        // We've finished sending one message
        // Let's clear the buffer and prepare the next one
        this->tx_buffer_index = 0;
        this->tx_buffer_length = 0;
        bzero(this->tx_buffer, sizeof(this->tx_buffer));

        if (this->_tx_messages.size() > 0)
        {
            // We have messages to send
            // Let's encode the first one and send it
            Message *message = this->_tx_messages[0];

            pb_ostream_t ostream = pb_ostream_from_buffer(this->tx_buffer, sizeof(this->tx_buffer));
            // ostream.errmsg = (const char *) nanopb_debug;
            if (pb_encode(&ostream, Message_fields, message))
            {
                this->tx_buffer_length = ostream.bytes_written;
                this->_tx_messages.erase(this->_tx_messages.begin());

                pb_release(Message_fields, message);
                delete message;
            }
        }
    }

    // if (this->tx_buffer_length == 0) {
    //     DEBUG("onRequest: no data to send\n");
    //     return;
    // }

    // if (this->tx_buffer_index < this->tx_buffer_length) {    
    //     uint8_t byte = this->tx_buffer[this->tx_buffer_index++];
    //     DEBUG("onRequest: sending byte tx[%02d]: %02x\n", this->tx_buffer_index-1, byte);
    //     this->_wire->write(byte);
    // }

    // if (this->tx_buffer_index == this->tx_buffer_length)
    // {
    //     DEBUG("onRequest: message sent\n");

    //     // We've finished sending one message
    //     // Let's clear the buffer and prepare the next one
    //     this->tx_buffer_index = 0;
    //     this->tx_buffer_length = 0;
    //     bzero(this->tx_buffer, sizeof(this->tx_buffer));

    //     if (this->_tx_messages.size() > 0)
    //     {
    //         // We have messages to send
    //         // Let's encode the first one and send it
    //         Message *message = this->_tx_messages[0];

    //         pb_ostream_t ostream = pb_ostream_from_buffer(this->tx_buffer, sizeof(this->tx_buffer));
    //         if (pb_encode(&ostream, Message_fields, message))
    //         {
    //             this->tx_buffer_length = ostream.bytes_written;
    //             this->_tx_messages.erase(this->_tx_messages.begin());
    //         }
    //     }
    // }
}

int RPCI2C::Read(Message *dest) 
{
    if (this->_rx_messages.size() > 0)
    {

        DEBUG("Posting message to RPC controller\n");

        // We have messages to read
        // Let's copy the first one into the destination
        Message *message = this->_rx_messages[0];
        pb_copy(Message_fields, message, dest);
        this->_rx_messages.erase(this->_rx_messages.begin());

        pb_release(Message_fields, message);
        delete message;

        return 0;
    }

    return -1;
}

int RPCI2C::Write(Message *src)
{
    // Need to check if we're sending a message
    // If we are, we need to queue this message

    // bool is_response = src->which_data == Message_response_tag;
    // bool is_record = src->data.response.result.which_value == InOut_record_tag;

    // if (is_response && is_record)
    // {
    //     DEBUG("RPCI2C::Write\n");
    //     DEBUG("Result ptr: %p\n", &src->data.response.result);
    //     DEBUG("Record Ptr: %p\n", &src->data.response.result.value.record);
    //     DEBUG("Record Samples Ptr: %p\n", src->data.response.result.value.record.samples);
    // }

    if (this->tx_buffer_length == 0)
    {
        DEBUG("Sending message immediately\n");

        // We're not sending a message
        // Let's encode this message and send it
        pb_ostream_t ostream = pb_ostream_from_buffer(this->tx_buffer, sizeof(this->tx_buffer));
        // ostream.errmsg = (const char *) nanopb_debug; 
        if (pb_encode(&ostream, Message_fields, src))
        {
            this->tx_buffer_length = ostream.bytes_written;

            // // Print the message for debugging
            // DEBUG("Length: %d\n", ostream.bytes_written);
            // for (int i = 0; i < ostream.bytes_written; i++)
            // {
            //     DEBUG("0x%x, ", this->tx_buffer[i]);
            // }
            // DEBUG("\n");
        }
    }
    else
    {
        // We're sending a message
        // Let's queue this message
        DEBUG("Queueing message. There are %d before it\n", this->_tx_messages.size());

        // Copy the message into a new buffer that we manage
        Message *message = new Message(); // Managed by the consumer of the `_tx_messages` vector (RPCI2C::onRequest, which frees)
        pb_copy(Message_fields, src, message);
        this->_tx_messages.push_back(message);
    }

    return 0;
}

int RPCI2C::loop()
{
    // bool read_data = false;
    // while (this->_wire->available() > 0)
    // {
    //     read_data = true;
    //     this->rx_buffer[this->rx_buffer_index++] = this->_wire->read();
    //     DEBUG("rx[%02d] = %02x\n", this->rx_buffer_index - 1, this->rx_buffer[this->rx_buffer_index - 1]);
    //     this->last_rx = millis();
    // }

    // if (read_data)
    // {
    //     Message *message = new Message();
    //     pb_istream_t istream = pb_istream_from_buffer(this->rx_buffer, this->rx_buffer_index);
    //     if (pb_decode(&istream, Message_fields, message))
    //     {
    //         DEBUG("Message received!\n");
    //         this->rx_buffer_index = 0;
    //         this->_rx_messages.push_back(message);

    //         DEBUG("%d messages pending to be read\n", this->_rx_messages.size());
    //     }
    //     else
    //     {
    //         free(message);
    //     }
    // }

    uint64_t time_since_last_rx = millis() - this->last_rx;
    if ((rx_buffer_index != 0) && (time_since_last_rx > 50))
    {
        DEBUG("Bus error: %dms\n", time_since_last_rx);
        // Assume there is a bus error and clear the RX buffer
        this->rx_buffer_index = 0;
        memset(this->rx_buffer, 0, sizeof(this->rx_buffer));
    }

    return 0;
}