#include <stdexcept>
//
#include <lux/util/log.hpp>
#include <lux/net/server/packet.hpp>
#include <lux/net/client/packet.hpp>
//
#include "net_client.hpp"

NetClient::NetClient(String const &server_hostname, net::Port port)
{
    enet_client = enet_host_create(nullptr, 1, 2, 0, 0);
    if(enet_client == nullptr)
    {
        throw std::runtime_error("couldn't create ENet client host");
    }
    ENetAddress server_addr;
    enet_address_set_host(&server_addr, server_hostname.c_str());
    server_addr.port = port;
    connect_to(&server_addr);
}

NetClient::~NetClient()
{
    disconnect();
    enet_host_destroy(enet_client);
}

bool NetClient::receive(net::server::Packet &sp)
{
    ENetEvent event;
    if(enet_host_service(enet_client, &event, TICK_TIMEOUT) > 0)
    {
        if(event.type == ENET_EVENT_TYPE_RECEIVE)
        {
            auto *pack = event.packet;
            net::clear_buffer(sp);
            deserializer.set_slice(pack->data, pack->data + pack->dataLength);
            deserializer >> sp;
            enet_packet_destroy(pack);
            return true;
        }
        else if(event.type == ENET_EVENT_TYPE_DISCONNECT)
        {
            throw std::runtime_error("lost connection to server");
        }
    }
    return false;
}

void NetClient::send(net::client::Packet &cp, U32 flags)
{
    //TODO packet buffer with enet_packet_resize
    //TODO move to shared?
    serializer.reserve(net::get_size(cp));
    serializer << cp;

    ENetPacket *pack = enet_packet_create(serializer.get(),
        serializer.get_used(), flags | ENET_PACKET_FLAG_NO_ALLOCATE);
    enet_peer_send(enet_server, 0, pack);
    enet_host_flush(enet_client);
    net::clear_buffer(cp);
}


void NetClient::connect_to(ENetAddress *server_addr)
{
    util::log("NET_CLIENT", util::INFO, "connecting to %u.%u.%u.%u:%u",
               server_addr->host        & 0xFF,
              (server_addr->host >>  8) & 0xFF,
              (server_addr->host >> 16) & 0xFF,
               server_addr->host >> 24,
               server_addr->port);
    enet_server = enet_host_connect(enet_client, server_addr, 1, 0);
    if(enet_server == nullptr)
    {
        throw std::runtime_error("couldn't create enet host");
    }
    ENetEvent event;
    if(enet_host_service(enet_client, &event, CONNECT_TIMEOUT) > 0)
    {
        if(event.type != ENET_EVENT_TYPE_CONNECT)
        {
            if(event.type == ENET_EVENT_TYPE_RECEIVE)
            {
                enet_packet_destroy(event.packet);
            }
            enet_peer_reset(enet_server);
            throw std::runtime_error("invalid server response");
        }
        else
        {
            util::log("NET_CLIENT", util::INFO, "connected to server");
        }
    }
    else
    {
        enet_peer_reset(enet_server);
        throw std::runtime_error("server connection timed out");
    }
}

void NetClient::disconnect()
{
    enet_peer_disconnect(enet_server, 0);
    ENetEvent event;
    while(enet_server->state != ENET_PEER_STATE_DISCONNECTED &&
          enet_host_service(enet_client, &event, DISCONNECT_TIMEOUT) > 0)
    {
        if(event.type == ENET_EVENT_TYPE_RECEIVE)
        {
            enet_packet_destroy(event.packet);
        }
        else if(event.type == ENET_EVENT_TYPE_DISCONNECT)
        {
            util::log("NET_CLIENT", util::INFO, "disconnected from server");
        }
    }
    enet_peer_reset(enet_server);
}


