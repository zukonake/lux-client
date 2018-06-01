#include <cstdlib>
//
#include <net/enet_handle.hpp>
#include <client.hpp>

int main()
{
    net::ENetHandle enet_handle;
    Client client("localhost", 31337, 128.0);
    return 0;
}
