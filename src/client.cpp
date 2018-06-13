#include <cassert>
#include <stdexcept>
//
#include <alias/int.hpp>
#include <util/log.hpp>
#include <net/ip.hpp>
#include <net/server_data.hpp>
#include <net/client_data.hpp>
#include <data/obj.hpp>
//
#include "client.hpp"

Client::Client(String const &server_hostname, net::Port port, double tick_rate, double fps) :
    enet_client(nullptr), //nullptr for null-check TODO?
    enet_server(nullptr), //
    tick_clock(util::TickClock::Duration(1.0 / tick_rate)), //TODO, should get info from server
    io_handler(default_config, fps),
    connected(false)
{
    enet_client = enet_host_create(NULL, 1, 1, 0, 0);
    if(enet_client == NULL)
    {
        throw std::runtime_error("couldn't create ENet client host");
    }
    ENetAddress server_addr;
    enet_address_set_host(&server_addr, server_hostname.c_str());
    server_addr.port = port;
    connect_to(&server_addr);
    run();
}

Client::~Client()
{
    disconnect();
    enet_host_destroy(enet_client);
}

void Client::connect_to(ENetAddress *server_addr)
{
    assert(!connected);
    util::log("CLIENT", util::INFO, "connecting to %u.%u.%u.%u:%u",
               server_addr->host        & 0xFF,
              (server_addr->host >>  8) & 0xFF,
              (server_addr->host >> 16) & 0xFF,
               server_addr->host >> 24,
               server_addr->port);
    enet_server = enet_host_connect(enet_client, server_addr, 1, 0);
    if(enet_server == NULL)
    {
        throw std::runtime_error("couldn't connect to server"); //TODO display addr?
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
            throw std::runtime_error("invalid server response");
        }
        util::log("CLIENT", util::INFO, "connected to server");
        connected = true;
    }
    else
    {
        enet_peer_reset(enet_server);
        throw std::runtime_error("server connection failed");
    }
    //TODO throws should probably be more descriptive
    //TODO integrate this function with handle_input()?
}

void Client::disconnect()
{
    assert(connected);
    enet_peer_disconnect(enet_server, 0);
    ENetEvent event;
    while(connected && enet_host_service(enet_client, &event, DISCONNECT_TIMEOUT) > 0)
    {
        switch(event.type)
        {
            case ENET_EVENT_TYPE_DISCONNECT:
            util::log("CLIENT", util::INFO, "disconnected from server");
            connected = false;
            break;

            case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(event.packet);
            break;

            default:
            break;
        }
    }
    enet_peer_reset(enet_server);
}

void Client::run()
{
    while(!io_handler.should_close())
    {
        tick_clock.start();
        tick();
        tick_clock.stop();
        tick_clock.synchronize();
    }
}

void Client::tick()
{
    handle_input();
    handle_output();
}

void Client::handle_input()
{
    ENetEvent event;
    while(enet_host_service(enet_client, &event, 0) > 0)
    {
        if(event.type != ENET_EVENT_TYPE_NONE)
        {
            //TODO
        }
        switch(event.type)
        {
            case ENET_EVENT_TYPE_RECEIVE:
                receive(event.packet);
                enet_packet_destroy(event.packet);
                break;
            default: break;
        }
    }

}


void Client::handle_output()
{
    io_handler.send(cd);
    Vector<U8> bytes;
    net::ClientData::serialize(cd, bytes);
    ENetPacket *packet = enet_packet_create(bytes.data(), bytes.size() * sizeof(uint8_t), 0);
    enet_peer_send(enet_server, 0, packet);
    enet_host_flush(enet_client);
}

void Client::receive(ENetPacket *packet)
{
    Vector<U8> bytes(packet->data, packet->data + packet->dataLength);
    sd.tiles.clear();
    net::ServerData::deserialize(sd, bytes);
    io_handler.receive(sd);
}

