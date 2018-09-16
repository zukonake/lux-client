#include <cstdlib>
#include <cstring>
//
#include <enet/enet.h>
//
#include <lux_shared/common.hpp>
#include <lux_shared/net/common.hpp>
#include <lux_shared/net/net_order.hpp>
#include <lux_shared/net/data.hpp>
#include <lux_shared/net/enet.hpp>
#include <lux_shared/util/tick_clock.hpp>
//
#include <map.hpp>

struct {
    //@CONSIDER null character instead
    //@RESEARCH whether the whole array actually gets filled
    Arr<U8, CLIENT_NAME_LEN> name = {0};
} conf;

struct {
    ENetHost* host;
    ENetPeer* peer;

    String server_name  = "unnamed";
    U16    tick_rate    = 0;
    bool   should_close = false;
} client;

void connect_to_server(char const* hostname, U16 port) {
    ENetAddress addr;
    if(enet_address_set_host(&addr, hostname) < 0) {
        LUX_FATAL("failed to set server hostname: %s", hostname);
    }
    addr.port = port;
    U8* ip = get_ip(addr);
    LUX_LOG("connecting to %u.%u.%u.%u:%u",
            ip[0], ip[1], ip[2], ip[3], port);
    client.peer = enet_host_connect(client.host, &addr, CHANNEL_NUM, 0);
    if(client.peer == nullptr) {
        LUX_FATAL("failed to connect to server");
    }

    { ///receive acknowledgement
        Uns constexpr MAX_TRIES = 10;
        Uns constexpr TRY_TIME  = 50; /// in milliseconds

        Uns tries = 0;
        ENetEvent event;
        do {
            if(enet_host_service(client.host, &event, TRY_TIME) > 0) {
                if(event.type != ENET_EVENT_TYPE_CONNECT) {
                    LUX_LOG("ignoring unexpected packet");
                    if(event.type == ENET_EVENT_TYPE_RECEIVE) {
                        enet_packet_destroy(event.packet);
                    }
                } else break;
            }
            if(tries >= MAX_TRIES) {
                enet_peer_reset(client.peer);
                LUX_FATAL("failed to acknowledge connection with server");
            }
            ++tries;
        } while(true);
        LUX_LOG("established connection with server");
        LUX_LOG("    after %zu/%zu tries", tries, MAX_TRIES);
    }

    { ///send init packet
        LUX_LOG("sending init packet");
        NetClientInit client_init_data = {
            {NET_VERSION_MAJOR, NET_VERSION_MINOR, NET_VERSION_PATCH},
            conf.name};
        if(send_init(client.peer, client.host, Slice<U8>(client_init_data))
            != LUX_RVAL_OK) {
            LUX_FATAL("failed to send init packet");
        }
        LUX_LOG("init packet sent successfully");
    }
    
    { ///receive init packet
        LUX_LOG("awaiting init packet");
        Uns constexpr MAX_TRIES = 10;
        Uns constexpr TRY_TIME  = 50; /// in milliseconds

        Uns tries = 0;
        U8 channel_id;
        ENetPacket* init_pack;
        do {
            enet_host_service(client.host, nullptr, TRY_TIME);
            init_pack = enet_peer_receive(client.peer, &channel_id);
            if(init_pack != nullptr) {
                if(channel_id == INIT_CHANNEL) {
                    break;
                } else {
                    LUX_LOG("ignoring unexpected packet on channel %u",
                            channel_id);
                    enet_packet_destroy(init_pack);
                }
            }
            if(tries >= MAX_TRIES) {
                LUX_FATAL("server did not send an init packet");
            }
            ++tries;
        } while(true);
        LUX_DEFER { enet_packet_destroy(init_pack); };
        LUX_LOG("received init packet after %zu/%zu tries", tries, MAX_TRIES);

        NetServerInit server_init_data;

        if(sizeof(server_init_data) != init_pack->dataLength) {
            LUX_FATAL("server sent invalid init packet with size %zu instead of"
                      " %zu", sizeof(server_init_data), init_pack->dataLength);
        }

        std::memcpy((U8*)&server_init_data, init_pack->data,
                    sizeof(server_init_data));
        client.tick_rate   = net_order(server_init_data.tick_rate);
        client.server_name = String((char const*)server_init_data.name.data());
        LUX_LOG("successfully connected to server %s",
                client.server_name.c_str());
        LUX_LOG("tick rate %u", client.tick_rate);
    }
}

void handle_tick(ENetPacket* in_pack) {

}

LUX_RVAL handle_signal(ENetPacket* in_pack) {
    if(in_pack->dataLength < 1) {
        LUX_LOG("couldn't read signal header, ignoring it");
        return LUX_RVAL_SERVER_SIGNAL;
    }

    SizeT static_size;
    SizeT dynamic_size;
    Slice<U8> dynamic_segment;
    NetServerSignal* signal = (NetServerSignal*)in_pack->data;

    { ///verify size
        ///we don't count the header
        SizeT static_dynamic_size = in_pack->dataLength - 1;
        SizeT needed_static_size;
        switch(signal->type) {
            case NetServerSignal::MAP_LOAD: {
                needed_static_size = sizeof(NetServerSignal::MapLoad);
            } break;
            default: {
                LUX_LOG("unexpected signal type, ignoring it");
                LUX_LOG("    type: %u", signal->type);
                LUX_LOG("    size: %zuB", in_pack->dataLength);
                return LUX_RVAL_SERVER_SIGNAL;
            }
        }
        if(static_dynamic_size < needed_static_size) {
            LUX_LOG("received packet static segment too small");
            LUX_LOG("    expected size: atleast %zuB", needed_static_size + 1);
            LUX_LOG("    size: %zuB", in_pack->dataLength);
            return LUX_RVAL_SERVER_SIGNAL;
        }
        static_size = needed_static_size;
        dynamic_size = static_dynamic_size - static_size;
        SizeT needed_dynamic_size;
        switch(signal->type) {
            case NetServerSignal::MAP_LOAD: {
                needed_dynamic_size = signal->map_load.chunks.len *
                                      sizeof(NetServerSignal::MapLoad::Chunk);
            } break;
            default: LUX_ASSERT(false);
        }
        if(dynamic_size != needed_static_size) {
            LUX_LOG("received packet dynamic segment size differs from expected");
            LUX_LOG("    expected size: %zuB", needed_dynamic_size +
                                               static_size + 1);
            LUX_LOG("    size: %zuB", in_pack->dataLength);
            return LUX_RVAL_SERVER_SIGNAL;
        }
        dynamic_segment.set((U8*)(in_pack->data + 1 + static_size), dynamic_size);
    }

    { ///parse the packet
        switch(signal->type) {
            case NetServerSignal::MAP_LOAD: {
                typedef NetServerSignal::MapLoad::Chunk NetChunk;
                Slice<NetChunk> chunks = dynamic_segment;

                for(Uns i = 0; i < chunks.len; ++i) {
                    load_chunk(chunks[i]);
                }
            } break;
            default: LUX_ASSERT(false);
        }
    }
    return LUX_RVAL_OK;
}

void do_tick() {
    { ///handle events
        ENetEvent event;
        while(enet_host_service(client.host, &event, 0) > 0) {
            if(event.type == ENET_EVENT_TYPE_DISCONNECT) {
                //@CONSIDER a more graceful reaction
                LUX_FATAL("connection closed by server");
            } else if(event.type == ENET_EVENT_TYPE_RECEIVE) {
                LUX_DEFER { enet_packet_destroy(event.packet); };
                if(event.channelID == TICK_CHANNEL) {
                    handle_tick(event.packet);
                } else if(event.channelID == TICK_CHANNEL) {
                    (void)handle_signal(event.packet);
                } else {
                    LUX_LOG("ignoring unexpected packet");
                    LUX_LOG("    channel: %u", event.channelID);
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    char const* server_hostname;
    U16 server_port;

    { ///read commandline args
        if(argc != 3) {
            LUX_FATAL("usage: %s SERVER_HOSTNAME SERVER_PORT", argv[0]);
        }
        U64 raw_server_port = std::atol(argv[2]);
        if(raw_server_port >= 1 << 16) {
            LUX_FATAL("invalid port %zu given", raw_server_port);
        }
        server_hostname = argv[1];
        server_port = raw_server_port;
    }

    LUX_LOG("initializing client");
    if(enet_initialize() < 0) {
        LUX_FATAL("couldn't initialize ENet");
    }
    LUX_DEFER {enet_deinitialize(); };

    { ///init client
        client.host = enet_host_create(nullptr, 1, CHANNEL_NUM, 0, 0);
        if(client.host == nullptr) {
            LUX_FATAL("couldn't initialize ENet host");
        }
    }
    LUX_DEFER { enet_host_destroy(client.host); };

    {
        U8 constexpr client_name[] = "lux-client";
        static_assert(sizeof(client_name) <= CLIENT_NAME_LEN);
        std::memcpy(conf.name.data(), client_name, sizeof(client_name));
    }

    connect_to_server(server_hostname, server_port);

    { ///main loop
        auto tick_len = util::TickClock::Duration(1.0 / (F64)client.tick_rate);
        util::TickClock clock(tick_len);
        while(!client.should_close) {
            clock.start();
            do_tick();
            clock.stop();
            auto remaining = clock.synchronize();
            if(remaining < util::TickClock::Duration::zero()) {
                LUX_LOG("tick overhead of %.2fs", std::abs(remaining.count()));
            }
        }
    }

    { ///disconnect from server
        Uns constexpr MAX_TRIES = 30;
        Uns constexpr TRY_TIME  = 25; ///in milliseconds

        enet_peer_disconnect(client.peer, 0);

        Uns tries = 0;
        ENetEvent event;
        do {
            if(enet_host_service(client.host, &event, TRY_TIME) > 0) {
                if(event.type == ENET_EVENT_TYPE_RECEIVE) {
                    LUX_LOG("ignoring unexpected packet");
                    enet_packet_destroy(event.packet);
                } else if(event.type == ENET_EVENT_TYPE_DISCONNECT) {
                    LUX_LOG("successfully disconnected from server");
                    break;
                }
            }
            if(tries >= MAX_TRIES) {
                LUX_LOG("failed to properly disconnect with server");
                LUX_LOG("forcefully terminating connection");
                enet_peer_reset(client.peer);
                break;
            }
            ++tries;
        } while(true);
    }
    return 0;
}
