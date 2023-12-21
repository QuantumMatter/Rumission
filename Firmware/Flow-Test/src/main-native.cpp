#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <rpc.pb.h>

#include <RPC.h>
#include <RPCSocket.h>

class Ping: public RPCHandler {
    public:
        Ping(const Message *message, const InOut *arg, InOut *result): RPCHandler(message, arg, result) {};
        ~Ping() {};

    public:
        int loop()
        {
            this->_result->which_value = InOut_integer_tag;
            this->_result->value.integer = this->_arg->value.integer + 1;
            return 0;
        };

        int cancel() { return 0; };
};

RPCHandler* RPCController::Dispatch(FUNCTION fun, const Message *message, const InOut *arg, InOut *result)
{
    switch (fun)
    {
        case FUNC_PING:
            return new Ping(message, arg, result);
        default:
            return NULL;
    }
}

int main(int argc, char **argv)
{
    printf("Hello, world!\n");

    RPCController controller(3);
    RPCSocket socket(8888);
    RPCTarget *target = controller.RegisterTarget(1, &socket);

    while (1)
    {
        controller.loop();

        usleep(1000);
    }

    return 0;
}