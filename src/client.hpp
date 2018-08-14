#pragma once

#include <lux/util/tick_clock.hpp>
#include <lux/net/server/packet.hpp>
#include <lux/net/client/packet.hpp>
//
#include <data/database.hpp>
#include <data/config.hpp>
#include <gl_initializer.hpp>
#include <net_client.hpp>
#include <io_client.hpp>

namespace net::server
{
    struct Tick;
    struct Conf;
    struct Msg;
}

class Client
{
    public:
    Client();
    private:
    bool should_run();

    void run();
    void receive_server_packets();
    void send_client_packets();

    void take_ss();
    void change_config();
    void display_msg();
    void init_from_server();

    data::Database db;
    data::Config conf;
    util::TickClock game_tick;

    net::server::Packet sp;
    net::client::Packet cp;

    GlInitializer  gl_initializer;
    NetClient net_client;
    IoClient   io_client;

    bool received_init;
    bool sent_init;
};
