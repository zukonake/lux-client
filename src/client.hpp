#pragma once

#include <enet/enet.h>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/string.hpp>
#include <lux/util/tick_clock.hpp>
#include <lux/net/port.hpp>
#include <lux/net/client/client_data.hpp>
#include <lux/net/server/server_data.hpp>
//
#include <io_handler.hpp>

class Client
{
public:
    Client(String const &server_hostname, net::Port port, double tick_rate, double fps);
    ~Client();
private:
    static const SizeT    CONNECT_TIMEOUT = 500;
    static const SizeT DISCONNECT_TIMEOUT = 500;

    void connect_to(ENetAddress *server_addr);
    void disconnect();
    void run();
    void tick();
    void handle_input();
    void handle_output();
    void receive(ENetPacket *packet); //TODO move those to a different class?
    ENetPacket *send();               //

    ENetHost       *enet_client;
    ENetPeer       *enet_server;
    util::TickClock tick_clock;
    net::ServerData sd;
    net::ClientData cd;
    IoHandler       io_handler;
    bool            connected;
};
