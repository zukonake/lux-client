#pragma once

#include <enet/enet.h>
//
#include <lux/alias/string.hpp>
#include <lux/net/port.hpp>
#include <lux/net/serializer.hpp>
#include <lux/net/deserializer.hpp>

namespace net::server { struct Packet; }
namespace net::client { struct Packet; }

class NetClient
{
public:
    NetClient(String const &server_hostname, net::Port port);
    ~NetClient();

    bool receive(net::server::Packet &);
    void send(net::client::Packet &, U32 flags);
private:
    void connect_to(ENetAddress *server_addr);
    void disconnect();

    static const SizeT    CONNECT_TIMEOUT = 1000;
    static const SizeT DISCONNECT_TIMEOUT = 500;
    static const SizeT       INIT_TIMEOUT = 500;
    static const SizeT       TICK_TIMEOUT = 0;

    ENetHost *enet_client;
    ENetPeer *enet_server;

    net::Serializer   serializer;
    net::Deserializer deserializer;
};
