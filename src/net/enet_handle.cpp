#include <stdexcept>
#include <enet/enet.h>
//
#include "enet_handle.hpp"

namespace net
{

ENetHandle::ENetHandle()
{
    if(enet_initialize() != 0) throw std::runtime_error("failed to initialize enet handle");
}

ENetHandle::~ENetHandle()
{
    enet_deinitialize();
}

}
