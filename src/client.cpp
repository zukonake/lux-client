#include <lux/util/log.hpp>
#include <lux/common/map.hpp>
//
#include "client.hpp"

Client::Client() :
    conf({&db,
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
          "glsl/vertex-2.1.glsl",
          "glsl/fragment-2.1.glsl",
          "glsl/interface-2.1",
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
          "glsl/vertex-es-2.0.glsl",
          "glsl/fragment-es-2.0.glsl",
          "glsl/interface-es-2.0",
#else
#   error "Unsupported GL variant selected"
#endif
          "tileset.png",
          "font.png",
          {800, 600},
          {16, 16},
          {8, 8},
          2,
          {
              "localhost",
              31337
          },
          8,
          6,
          120.f,
          {0.3, 0.4, 0.5, 1.0},
          "lux client"}),
    game_tick(util::TickClock::Duration(1)),
    gl_initializer(conf.window_size),
    net_client(conf.server.hostname, conf.server.port),
    io_client(gl_initializer.get_window(), conf),
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
    constexpr U32 TICK_DELTA_SAMPLING = 64;
    U32 tick_sample = 0;
    F64 delta_avg = 0;
    while(should_run())
    {
        game_tick.start();
        if(tick_sample >= TICK_DELTA_SAMPLING)
        {
            io_client.set_tick_time(delta_avg / (F64)TICK_DELTA_SAMPLING);
            delta_avg = 0;
            tick_sample = 0;
        }
        receive_server_packets();
        send_client_packets();
        game_tick.stop();
        auto delta = game_tick.synchronize();
        delta_avg += delta.count();
        if(delta < util::TickClock::Duration::zero())
        {
            util::log("CLIENT", util::WARN, "tick overhead of %.2f ticks",
                      std::abs(delta / game_tick.get_tick_len()));
        }
        tick_sample += 1;
    }
}

void Client::receive_server_packets()
{
    while(net_client.receive(sp))
    {
        take_ss();
        io_client.dispatch_signal(sp);
    }
    io_client.dispatch_tick(sp.tick);
}

void Client::send_client_packets()
{
    if(!sent_init) //TODO move to Client::give_cs?
    {
        util::log("SERVER", util::INFO, "initializing to server");
        cp.type = net::client::Packet::INIT;
        cp.init.conf.load_range = conf.load_range;
        std::copy(conf.client_name.begin(), conf.client_name.end(),
                  std::back_inserter(cp.init.client_name));

        net_client.send(cp, ENET_PACKET_FLAG_RELIABLE);
        sent_init = true;
    }
    else
    {
        while(io_client.gather_signal(cp))
        {
            net_client.send(cp, ENET_PACKET_FLAG_RELIABLE);
        }
        cp.type = net::client::Packet::TICK;
        io_client.gather_tick(cp.tick);
        net_client.send(cp, ENET_PACKET_FLAG_UNSEQUENCED);
    }
}

void Client::take_ss()
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
    if(si.chunk_size != CHK_SIZE) //TODO check ver?
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
