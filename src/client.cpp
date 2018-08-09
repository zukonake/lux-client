#include <lux/util/log.hpp>
#include <lux/common/chunk.hpp>
//
#include <data/config.hpp>
#include <data/obj.hpp>
#include "client.hpp"

Client::Client() :
    conf(default_config),
    game_tick(util::TickClock::Duration(1)),
    net_client(conf.server.hostname, conf.server.port),
    io_client(conf),
    received_init(false),
    sent_init(false)
{
    sp.tick = {{}, {0, 0, 0}};
    run();
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
        receive_server_packets();
        send_client_packets();
        game_tick.stop();
        auto delta = game_tick.synchronize();
        if(delta < util::TickClock::Duration::zero())
        {
            util::log("CLIENT", util::WARN, "tick overhead of %.2f ticks",
                      std::abs(delta / game_tick.get_tick_len()));
        }
    }
}

void Client::receive_server_packets()
{
    while(net_client.receive(sp))
    {
        take_server_signal();
        io_client.take_server_signal(sp);
    }
    io_client.take_server_tick(sp.tick);
}

void Client::send_client_packets()
{
    if(!sent_init) //TODO move to Client::give_client_signal?
    {
        util::log("SERVER", util::INFO, "initializing to server");
        cp.type = net::client::Packet::INIT;
        cp.init.conf.view_range = conf.view_range;
        std::copy(conf.client_name.begin(), conf.client_name.end(),
                  std::back_inserter(cp.init.client_name));
        
        net_client.send(cp, ENET_PACKET_FLAG_RELIABLE);
        sent_init = true;
    }
    else
    {
        while(io_client.give_client_signal(cp))
        {
            net_client.send(cp, ENET_PACKET_FLAG_RELIABLE);
        }
        io_client.give_client_tick(cp);
        net_client.send(cp, ENET_PACKET_FLAG_UNSEQUENCED);
    }
}

void Client::take_server_signal()
{
    if(!received_init) //TODO move to ctor?
    {
        if(sp.type == net::server::Packet::INIT)
        {
            init_from_server();
            received_init = true;
        }
        else
        {
            throw std::runtime_error("server has not sent init data");
        }
    }
    else if(sp.type == net::server::Packet::CONF)
    {
        change_config();
    }
    else if(sp.type == net::server::Packet::MSG)
    {
        display_msg();
    }
}

void Client::init_from_server()
{
    auto const &si = sp.init;
    util::log("CLIENT", util::INFO, "received initialization data");
    if(si.chunk_size != chunk::SIZE) //TODO check ver?
    {
        throw std::runtime_error("incompatible chunk size");
    }
    String server_name(si.server_name.begin(), si.server_name.end());
    util::log("CLIENT", util::INFO, "server name: %s", server_name);
    sp.conf = sp.init.conf;
    change_config();
}

void Client::change_config()
{
    auto const &sc = sp.conf;
    util::log("CLIENT", util::INFO, "changing configuration");
    util::log("CLIENT", util::INFO, "tick rate: %.2f", sc.tick_rate);
    game_tick.set_rate(util::TickClock::Duration(1.f / sc.tick_rate));
}

void Client::display_msg()
{
    auto const &sm = sp.msg;
    String msg_str(sm.log_msg.begin(), sm.log_msg.end());
    util::log("CLIENT", sm.log_level, "message from server: %s", msg_str);
}
