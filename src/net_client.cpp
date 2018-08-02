#include <stdexcept>
//
#include <lux/util/log.hpp>
#include <lux/serial/server_init_data.hpp>
#include <lux/serial/server_data.hpp>
#include <lux/serial/client_data.hpp>
//
#include "net_client.hpp"

NetClient::NetClient(String const &server_hostname, net::Port port)
{
    enet_client = enet_host_create(nullptr, 1, 1, 0, 0);
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
        throw std::runtime_error("couldn't connect to server");
    }
    ENetEvent event;
    if(enet_host_service(enet_client, &event, CONNECT_TIMEOUT) > 0)
    {
        if(event.type != ENET_EVENT_TYPE_CONNECT)
        {
            enet_peer_reset(enet_server);
            if(event.type == ENET_EVENT_TYPE_DISCONNECT)
            {
                throw std::runtime_error("server refused connection");
            }
            else
            {
                throw std::runtime_error("invalid server response");
            }
        }
        else
        {
            util::log("NET_CLIENT", util::INFO, "connected to server");
        }
    }
    else
    {
        enet_peer_reset(enet_server);
        throw std::runtime_error("server connection failed");
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
    }
    enet_peer_reset(enet_server);
    util::log("NET_CLIENT", util::INFO, "disconnected from server");
}

void NetClient::get_server_init_data(serial::ServerInitData &sid)
{
    ENetEvent event;
    while(enet_host_service(enet_client, &event, INIT_TIMEOUT) > 0)
    {
        if(event.type == ENET_EVENT_TYPE_RECEIVE)
        {
            auto *packet = event.packet;
            deserializer.set_slice(packet->data,
                                   packet->data + packet->dataLength);
            deserializer >> sid;
            enet_packet_destroy(packet);
            return;
        }
    }
    throw std::runtime_error("couldn't fetch server init data");
}

void NetClient::get_server_data(serial::ServerData &sd)
{
    ENetEvent event;
    if(enet_host_service(enet_client, &event, 5) > 0) //TODO timeout
    {
        sd.chunks.clear();
        sd.entities.clear();
        if(event.type == ENET_EVENT_TYPE_RECEIVE)
        {
            auto *packet = event.packet;
            deserializer.set_slice(packet->data,
                                   packet->data + packet->dataLength);
            deserializer >> sd;
            enet_packet_destroy(packet);
        }
    }
    else
    {
        util::log("NET_CLIENT", util::WARN, "tick packet lost");
    }
}

void NetClient::set_client_data(serial::ClientData const &cd)
{
    serializer.reserve(serial::get_size(cd));
    serializer << cd;
    ENetPacket *packet =
        enet_packet_create(serializer.get(), serializer.get_size(), 0);
    enet_peer_send(enet_server, 0, packet);
    enet_host_flush(enet_client); //TODO should this be here? benchmark
}
