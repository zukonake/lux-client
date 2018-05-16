#include <stdexcept>
#include <ncurses.h>
//
#include "client.hpp"

Client::Client(std::string server_hostname, net::Port port, double tick_rate) :
    enet_client(nullptr), //nullptr for null-check
    enet_server(nullptr), //
    tick_clock(util::TickClock::Duration(1.0 / tick_rate)) //TODO, should get info from server
{
    enet_client = enet_host_create(NULL, 1, 1, 0, 0);
    if(enet_client == NULL)
    {
        throw std::runtime_error("couldn't create ENet client host");
    }
    ENetAddress server_addr;
    enet_address_set_host(&server_addr, server_hostname.c_str());
    server_addr.port = port;
    connect_to(server_addr);
    run();
}

Client::~Client()
{
    enet_host_destroy(enet_client);
}

void Client::connect_to(ENetAddress server_addr)
{
    enet_server = enet_host_connect(enet_client, &server_addr, 1, 0);
    if(enet_server == NULL)
    {
        throw std::runtime_error("couldn't connect to server"); //TODO display addr?
    }
    ENetEvent event;
    if(enet_host_service(enet_client, &event, 5000) > 0) //TODO magic number
    {
        if(event.type == ENET_EVENT_TYPE_DISCONNECT)
        {
            throw std::runtime_error("server refused connection");
        }
        else if(event.type != ENET_EVENT_TYPE_CONNECT)
        {
            throw std::runtime_error("invalid server response");
        }
        printw("client connected\n"); //TODO
        refresh();
    }
    else
    {
        enet_peer_reset(enet_server);
        throw std::runtime_error("server connection failed");
    }
    //TODO throws should probably be more descriptive
}

void Client::disconnect()
{
    enet_peer_disconnect(enet_server, 0);
    ENetEvent event;
    while(enet_host_service(enet_client, &event, 3000) > 0) //TODO magic number
    {
        switch(event.type)
        {
            case ENET_EVENT_TYPE_DISCONNECT:
            enet_server = nullptr; //for reset peer null-check
            //TODO debug message?
            break;

            default:
            enet_packet_destroy(event.packet);
            break;
        }
    }
    if(enet_server != nullptr) enet_peer_reset(enet_server);
}

void Client::run()
{
    while(true) //TODO
    {
        tick_clock.start();
        tick();
        tick_clock.stop();
        tick_clock.synchronize();
    }
}

void Client::tick()
{

}
