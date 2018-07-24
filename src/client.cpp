#include <cmath>
//
#include <lux/util/log.hpp>
#include <lux/serial/server_init_data.hpp>
//
#include <data/config.hpp>
#include <data/obj.hpp>
#include "client.hpp"

Client::Client() :
    conf(default_config),
    game_tick(util::TickClock::Duration::zero()),
    net_client(conf.server.hostname, conf.server.port),
    io_client(conf, 60.0)
{
    init_from_server();
    run();
}

void Client::init_from_server()
{
    serial::ServerInitData sid;
    net_client.get_server_init_data(sid);
    game_tick.set_rate(util::TickClock::Duration(1.f / sid.tick_rate));
}

bool Client::should_run()
{
    return !io_client.should_close();
}

void Client::run()
{
    while(should_run())
    {
        game_tick.start();
        tick();
        game_tick.stop();
        auto delta = game_tick.synchronize();
        if(delta < util::TickClock::Duration::zero())
        {
            util::log("CLIENT", util::WARN, "game loop overhead of of %f"
                      "seconds", std::abs(delta.count()));
        }
    }
}

void Client::tick()
{
    net_client.get_server_data(sd);
    io_client.set_server_data(sd);
    io_client.get_client_data(cd);
    net_client.set_client_data(cd);
}
