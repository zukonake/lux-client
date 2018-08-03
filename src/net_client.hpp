#pragma once

#include <enet/enet.h>
//
#include <lux/alias/string.hpp>
#include <lux/net/port.hpp>
#include <lux/serial/serializer.hpp>
#include <lux/serial/deserializer.hpp>

namespace serial
{
    struct ServerInitData;
    struct ServerData;
    struct ClientData;
}

class NetClient
{
public:
    NetClient(String const &server_hostname, net::Port port);
    ~NetClient();

    void get_server_init_data(serial::ServerInitData &sid);
    void get_server_data(serial::ServerData       &sd);
    void set_client_data(serial::ClientData const &cd);
private:
    void connect_to(ENetAddress *server_addr);
    void disconnect();

    static const SizeT    CONNECT_TIMEOUT = 500;
    static const SizeT DISCONNECT_TIMEOUT = 500;
    static const SizeT       INIT_TIMEOUT = 500;

    ENetHost       *enet_client;
    ENetPeer       *enet_server;

    serial::Serializer     serializer;
    serial::Deserializer deserializer;
};
