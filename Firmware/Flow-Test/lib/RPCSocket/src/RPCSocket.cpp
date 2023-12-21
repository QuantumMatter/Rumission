#include <pb_encode.h>
#include <pb_decode.h>

#include "RPCSocket.h"

#include <rpc.pb.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <fcntl.h>

RPCSocket::RPCSocket(uint16_t port): RPCIO()
{
    this->_client_fd = -1;

    // Create a socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    this->_server_fd = fd;
    if (this->_server_fd == -1) {
        printf("Failed to create socket\n");
        return;
    }

    // Bind the socket to a specific address and port
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {
            .s_addr = htonl(INADDR_LOOPBACK),
        },
    };

    if (bind(this->_server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        printf("Failed to bind socket\n");
        this->_server_fd = -1;
        return;
    }

    // Set the socket to non-blocking mode
    int flags = fcntl(this->_server_fd, F_GETFL, 0);
    if (flags == -1) {
        printf("Failed to get socket flags\n");
        this->_server_fd = -1;
        return;
    }
    flags |= O_NONBLOCK;
    if (fcntl(this->_server_fd, F_SETFL, flags) == -1) {
        printf("Failed to set socket to non-blocking mode\n");
        this->_server_fd = -1;
        return;
    }

    // Listen for incoming connections
    if (listen(this->_server_fd, 1) == -1) {
        printf("Failed to listen on socket\n");
        this->_server_fd = -1;
        return;
    }
}

RPCSocket::~RPCSocket()
{
    if (this->_client_fd != -1) {
        close(this->_client_fd);
    }

    if (this->_server_fd != -1) {
        close(this->_server_fd);
    }
}

int RPCSocket::Read(Message *dest)
{
    if (this->_rx_messages.size() == 0) {
        return -1;
    }

    Message *message = this->_rx_messages[0];
    pb_copy(Message_fields, message, dest);

    this->_rx_messages.erase(this->_rx_messages.begin());
    delete message;

    return 0;
}

int RPCSocket::Write(Message *src)
{
    if (this->_client_fd == -1) {
        return -1;
    }

    uint8_t buffer[2048];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&ostream, Message_fields, src)) {
        return -1;
    }

    ssize_t rc = write(this->_client_fd, buffer, ostream.bytes_written);
    if (rc != ostream.bytes_written) {
        printf("Failed to write to socket\n");
        return -1;
    }

    // delete src;
    return 0;
}


int RPCSocket::loop()
{
    if (this->_server_fd == -1) {
        return -1;
    }

    if (this->_client_fd == -1)
    {
        // Try to accept a new connection
        this->_client_fd = accept(this->_server_fd, NULL, NULL);
        if (this->_client_fd == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // No incoming connections, sleep for a bit
                // pass
            } else {
                printf("Failed to accept connection\n");
                return -1;
            }
        }
        else
        {
            // Make the socket non-blocking
            int flags = fcntl(this->_client_fd, F_GETFL, 0);
            if (flags == -1) {
                printf("Failed to get socket flags\n");
                return -1;
            }
            flags |= O_NONBLOCK;
            if (fcntl(this->_client_fd, F_SETFL, flags) == -1) {
                printf("Failed to set socket to non-blocking mode\n");
                return -1;
            }
        }
    }

    if (this->_client_fd != -1)
    {
        bool read_data = false;
        uint8_t byte = 0;
        ssize_t read_size = 0;
        while (true)
        {
            read_size = read(this->_client_fd, &byte, 1);
            if (read_size > 0)
            {
                read_data = true;
                this->rx_buffer[this->rx_buffer_index++] = byte;
            }
            else if (read_size == 0)
            {
                // Client disconnected, reset the client_fd
                close(this->_client_fd);
                this->_client_fd = -1;
                break;
            }
            else if (errno != EWOULDBLOCK && errno != EAGAIN)
            {
                // Error occurred while reading from socket
                printf("Failed to read from socket: %d\n", errno);
                return -errno;
            }
            else
            {
                break;
            }
        }

        if (read_data)
        {
            Message *message = new Message();
            if (!message) return -1;

            pb_istream_t istream = pb_istream_from_buffer(this->rx_buffer, this->rx_buffer_index);
            if (pb_decode(&istream, Message_fields, message))
            {
                this->_rx_messages.push_back(message);
                printf("Received message!\n");
                this->rx_buffer_index = 0;
            }
            else
            {
                delete message;
                return -1;
            }
        }
    }

    return 0;
}