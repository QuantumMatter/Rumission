#include "RPC.h"


RPCController::RPCController(uint32_t id) : _id(id) {}
RPCController::~RPCController() {}

RPCTarget* RPCController::RegisterTarget(uint32_t id, RPCIO *io)
{
    RPCTarget *target = new RPCTarget(this, id, io);
    if (!target) return NULL;

    this->_targets.push_back(target);
    return target;
}

int RPCController::loop()
{
    for (auto target : this->_targets)
    {
        target->loop();
    }
    return 0;
}