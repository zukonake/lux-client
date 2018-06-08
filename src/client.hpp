#pragma once

#include <enet/enet.h>
//
#include <alias/int.hpp>
#include <alias/string.hpp>
#include <util/tick_clock.hpp>
#include <net/port.hpp>
#include <net/server_data.hpp>
#include <io_handler.hpp>

class Client
{
public:
    Client(String server_hostname, net::Port port, double tick_rate, double fps);
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
