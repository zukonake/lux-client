#pragma once

#include <cstddef>
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
    static const std::size_t    CONNECT_TIMEOUT = 5000;
    static const std::size_t DISCONNECT_TIMEOUT = 5000;

    void connect_to(ENetAddress *server_addr);
    void disconnect();
    void run();
    void tick();

    ENetHost       *enet_client;
    ENetPeer       *enet_server;
    util::TickClock tick_clock;
};
