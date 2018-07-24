#pragma once

#include <lux/util/tick_clock.hpp>
#include <lux/serial/server_data.hpp>
#include <lux/serial/client_data.hpp>
//
#include <net_client.hpp>
#include <io_client.hpp>

namespace data { struct Config; }

class Client
{
    public:
    Client();
    private:
    bool should_run();

    void init_from_server();
    void run();
    void tick();

    data::Config const &conf;
    util::TickClock game_tick;

    NetClient net_client;
    IoClient   io_client;

    serial::ServerData sd;
    serial::ClientData cd;
};
