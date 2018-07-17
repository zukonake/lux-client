#include <lux/net/enet_handle.hpp>
//
#include <client.hpp>

int main()
{
    net::ENetHandle enet_handle; //TODO place into client?
    Client client("localhost", 31337, 64.0, 60);
    return 0;
}
