#pragma once

#include <string>

#include <enet/enet.h>

#include <util/tick_clock.hpp>
#include <net/port.hpp>

class Client
{
public:
    Client(std::string server_hostname, net::Port port, double tick_rate);
    ~Client();
private:
    void connect_to(ENetAddress server_addr);
    void disconnect();
    void run();
    void tick();

    ENetHost       *enet_client;
    ENetPeer       *enet_server;
    util::TickClock tick_clock;
};
