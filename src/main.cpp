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
#include <lux_shared/util/packer.hpp>
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
    F32    load_rad     = 3;
    bool   should_close = false;
    HashSet<ChkPos, util::Packer<ChkPos>> requested_chunks;
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
        LUX_LOG("established connection with server after %zu/%zu tries",
                tries, MAX_TRIES);
    }

    { ///send init packet
        LUX_LOG("sending init packet");
        ENetPacket* pack;
        if(create_reliable_pack(pack, sizeof(NetClientInit)) != LUX_OK) {
            LUX_FATAL("failed to create init packet");
        }
        NetClientInit* init = (NetClientInit*)pack->data;
        init->net_ver = {NET_VERSION_MAJOR, NET_VERSION_MINOR, NET_VERSION_PATCH};
        init->name    = conf.name;
        if(send_packet(client.peer, pack, INIT_CHANNEL)!= LUX_OK) {
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

LUX_MAY_FAIL handle_tick(ENetPacket* in_pack) {
    if(in_pack->dataLength != sizeof(NetServerTick)) {
        LUX_LOG("received tick packet has unexpected size");
        LUX_LOG("    expected size: %zuB", sizeof(NetServerTick));
        LUX_LOG("    size: %zuB", in_pack->dataLength);
        return LUX_FAIL;
    }

    NetServerTick *tick = (NetServerTick*)in_pack->data;

    {
        ChkPos const& center = to_chk_pos(net_order(tick->player_pos));
        DynArr<ChkPos> requests;
        ChkPos iter;
        for(iter.z  = center.z - client.load_rad;
            iter.z <= center.z + client.load_rad;
            iter.z++) {
            for(iter.y  = center.y - client.load_rad;
                iter.y <= center.y + client.load_rad;
                iter.y++) {
                for(iter.x  = center.x - client.load_rad;
                    iter.x <= center.x + client.load_rad;
                    iter.x++) {
                    if(glm::distance((Vec3F)iter, (Vec3F)center)
                           <= client.load_rad &&
                       client.requested_chunks.count(iter) == 0 &&
                       !is_chunk_loaded(iter)) {
                        requests.emplace_back(net_order(iter));
                    }
                }
            }
        }
        if(requests.size() > 0) {
            SizeT constexpr static_sz = 1 + sizeof(NetClientSignal::MapRequest);
            SizeT          dynamic_sz = requests.size() * sizeof(ChkPos);

            ENetPacket* pack;
            if(create_reliable_pack(pack, static_sz + dynamic_sz) != LUX_OK) {
                return LUX_FAIL;
            }
            NetClientSignal* signal = (NetClientSignal*)pack->data;
            signal->type = NetClientSignal::MAP_REQUEST;
            signal->map_request.requests.len = net_order<U32>(requests.size());
            std::memcpy(pack->data + static_sz, requests.data(), dynamic_sz);
            if(send_packet(client.peer, pack, SIGNAL_CHANNEL) != LUX_OK) {
                return LUX_FAIL;
            }
        }
    }
    return LUX_OK;
}

//@CONSIDER shared version
LUX_MAY_FAIL handle_signal(ENetPacket* in_pack) {
    SizeT sz = in_pack->dataLength;
    if(sz < 1) {
        LUX_LOG("couldn't read signal header, ignoring signal");
        return LUX_FAIL;
    }

    SizeT static_sz;
    SizeT dynamic_sz;
    Slice<U8> dynamic_segment;
    NetServerSignal* signal = (NetServerSignal*)in_pack->data;

    { ///verify size
        SizeT needed_static_sz;
        switch(signal->type) {
            case NetServerSignal::MAP_LOAD: {
                needed_static_sz = 1 + sizeof(NetServerSignal::MapLoad);
            } break;
            default: {
                LUX_LOG("unexpected signal type, ignoring signal");
                LUX_LOG("    type: %u", signal->type);
                LUX_LOG("    size: %zuB", in_pack->dataLength);
                return LUX_FAIL;
            }
        }
        if(sz < needed_static_sz) {
            LUX_LOG("received packet static segment too small");
            LUX_LOG("    expected size: atleast %zuB", needed_static_sz);
            LUX_LOG("    size: %zuB", sz);
            return LUX_FAIL;
        }
        static_sz = needed_static_sz;
        dynamic_sz = sz - static_sz;
        SizeT needed_dynamic_sz;
        switch(signal->type) {
            case NetServerSignal::MAP_LOAD: {
                needed_dynamic_sz = signal->map_load.chunks.len *
                                    sizeof(NetServerSignal::MapLoad::Chunk);
            } break;
            default: LUX_ASSERT(false);
        }
        if(dynamic_sz != needed_dynamic_sz) {
            LUX_LOG("received packet dynamic segment size differs from expected");
            LUX_LOG("    expected size: %zuB", needed_dynamic_sz + static_sz);
            LUX_LOG("    size: %zuB", in_pack->dataLength);
            return LUX_FAIL;
        }
        dynamic_segment.set((U8*)(in_pack->data + static_sz), dynamic_sz);
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
    return LUX_OK;
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
                    if(handle_tick(event.packet) != LUX_OK) continue;
                } else if(event.channelID == SIGNAL_CHANNEL) {
                    if(handle_signal(event.packet) != LUX_OK) continue;
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
