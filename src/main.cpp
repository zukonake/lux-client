#include <cstdlib>
#include <cmath>
#include <cstring>
//
#include <enet/enet.h>
#define GLM_FORCE_PURE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/net/common.hpp>
#include <lux_shared/net/data.hpp>
#include <lux_shared/net/enet.hpp>
#include <lux_shared/net/serial.hpp>
#include <lux_shared/util/packer.hpp>
#include <lux_shared/util/tick_clock.hpp>
//
#include <db.hpp>
#include <map.hpp>
#include <console.hpp>
#include <rendering.hpp>

struct {
    //@CONSIDER null character instead
    //@RESEARCH whether the whole array actually gets filled
    Arr<U8, CLIENT_NAME_LEN> name = {0};
    Vec2U window_size = {800, 600};
} conf;

struct {
    ENetHost* host;
    ENetPeer* peer;

    String server_name  = "unnamed";
    U16    tick_rate    = 0;
    bool   should_close = false;
    VecSet<ChkPos> sent_requests;
} client;

EntityVec player_pos = {0, 0, 0};

void window_resize_cb(GLFWwindow* window, int win_w, int win_h)
{
    (void)window;
    LUX_LOG("window size change to %ux%u", win_w, win_h);
    glViewport(0, 0, win_w, win_h);
    console_window_resize_cb(win_w, win_h);
}

void key_cb(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    //@TODO if console captures the key, it shouldn't control the entity etc.
    //@CONSIDER a centralized input system
    console_key_cb(key, scancode, action, mods);
}

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
        ENetPacket* out_pack;
        if(create_reliable_pack(out_pack, sizeof(NetCsInit)) != LUX_OK) {
            LUX_FATAL("failed to create init packet");
        }
        U8* iter = out_pack->data;
        serialize(&iter, NET_VERSION_MAJOR);
        serialize(&iter, NET_VERSION_MINOR);
        serialize(&iter, NET_VERSION_PATCH);
        serialize(&iter, conf.name);
        LUX_ASSERT(iter == out_pack->data + out_pack->dataLength);

        if(send_packet(client.peer, out_pack, INIT_CHANNEL)!= LUX_OK) {
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
        ENetPacket* in_pack;
        do {
            enet_host_service(client.host, nullptr, TRY_TIME);
            in_pack = enet_peer_receive(client.peer, &channel_id);
            if(in_pack != nullptr) {
                if(channel_id == INIT_CHANNEL) {
                    break;
                } else {
                    LUX_LOG("ignoring unexpected packet on channel %u",
                            channel_id);
                    enet_packet_destroy(in_pack);
                }
            }
            if(tries >= MAX_TRIES) {
                LUX_FATAL("server did not send an init packet");
            }
            ++tries;
        } while(true);
        LUX_DEFER { enet_packet_destroy(in_pack); };
        LUX_LOG("received init packet after %zu/%zu tries", tries, MAX_TRIES);

        {   NetSsInit ss_init;

            U8 const* iter = in_pack->data;
            if(check_pack_size(sizeof(NetSsInit), iter, in_pack) != LUX_OK) {
                LUX_FATAL("failed to receive init packet");
            }

            deserialize(&iter, &ss_init.name);
            deserialize(&iter, &ss_init.tick_rate);
            LUX_ASSERT(iter == in_pack->data + in_pack->dataLength);

            client.server_name = String((char const*)ss_init.name);
            client.tick_rate = ss_init.tick_rate;
        }
        LUX_LOG("successfully connected to server %s",
                client.server_name.c_str());
        LUX_LOG("tick rate %u", client.tick_rate);
    }
}

LUX_MAY_FAIL handle_tick(ENetPacket* in_pack) {
    U8 const* iter = in_pack->data;
    if(check_pack_size(sizeof(NetSsTick), iter, in_pack) != LUX_OK) {
        LUX_LOG("failed to handle tick");
        return LUX_FAIL;
    }

    deserialize(&iter, &player_pos);
    LUX_ASSERT(iter == in_pack->data + in_pack->dataLength);
    return LUX_OK;
}

LUX_MAY_FAIL handle_signal(ENetPacket* in_pack) {
    U8 const* iter = in_pack->data;
    if(check_pack_size_atleast(sizeof(NetSsSgnl::Header), iter, in_pack)
           != LUX_OK) {
        LUX_LOG("couldn't read signal header");
        return LUX_FAIL;
    }

    NetSsSgnl sgnl;
    deserialize(&iter, (U8*)&sgnl.header);

    if(sgnl.header >= NetSsSgnl::HEADER_MAX) {
        LUX_LOG("unexpected signal header %u", sgnl.header);
        return LUX_FAIL;
    }

    SizeT expected_stt_sz;
    switch(sgnl.header) {
        case NetSsSgnl::MAP_LOAD: {
            expected_stt_sz = sizeof(NetSsSgnl::MapLoad);
        } break;
        case NetSsSgnl::LIGHT_UPDATE: {
            expected_stt_sz = sizeof(NetSsSgnl::LightUpdate);
        } break;
        default: LUX_UNREACHABLE();
    }
    if(check_pack_size_atleast(expected_stt_sz, iter, in_pack) != LUX_OK) {
        LUX_LOG("couldn't read static segment");
        return LUX_FAIL;
    }

    SizeT expected_dyn_sz;
    switch(sgnl.header) {
        case NetSsSgnl::MAP_LOAD: {
            deserialize(&iter, &sgnl.map_load.chunks.len);
            expected_dyn_sz = sgnl.map_load.chunks.len *
                sizeof(NetSsSgnl::MapLoad::Chunk);
        } break;
        case NetSsSgnl::LIGHT_UPDATE: {
            deserialize(&iter, &sgnl.light_update.chunks.len);
            expected_dyn_sz = sgnl.light_update.chunks.len *
                sizeof(NetSsSgnl::LightUpdate::Chunk);
        } break;
        default: LUX_UNREACHABLE();
    }
    if(check_pack_size(expected_dyn_sz, iter, in_pack) != LUX_OK) {
        LUX_LOG("couldn't read dynamic segment");
        return LUX_FAIL;
    }

    { ///parse the packet
        switch(sgnl.header) {
            case NetSsSgnl::MAP_LOAD: {
                typedef NetSsSgnl::MapLoad::Chunk NetChunk;
                Slice<NetChunk> chunks;
                chunks.len = sgnl.map_load.chunks.len;
                chunks.beg = lux_alloc<NetChunk>(chunks.len);
                LUX_DEFER { lux_free(chunks.beg); };

                for(Uns i = 0; i < chunks.len; ++i) {
                    deserialize(&iter, &chunks[i].pos);
                    deserialize(&iter, &chunks[i].voxels);
                    deserialize(&iter, &chunks[i].light_lvls);
                    load_chunk(chunks[i]);
                }
            } break;
            case NetSsSgnl::LIGHT_UPDATE: {
                typedef NetSsSgnl::LightUpdate::Chunk NetChunk;
                Slice<NetChunk> chunks;
                chunks.len = sgnl.light_update.chunks.len;
                chunks.beg = lux_alloc<NetChunk>(chunks.len);
                LUX_DEFER { lux_free(chunks.beg); };

                for(Uns i = 0; i < chunks.len; ++i) {
                    deserialize(&iter, &chunks[i].pos);
                    deserialize(&iter, &chunks[i].light_lvls);
                    light_update(chunks[i]);
                }
            } break;
            default: LUX_UNREACHABLE();
        }
    }
    LUX_ASSERT(iter == in_pack->data + in_pack->dataLength);
    return LUX_OK;
}

//@TODO err handling
void do_tick() {
    { ///handle events
        ENetEvent event;
        while(enet_host_service(client.host, &event, 0) > 0) {
            if(event.type == ENET_EVENT_TYPE_DISCONNECT) {
                LUX_LOG("connection closed by server");
                client.should_close = true;
                return;
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

    ///send map request signal
    {   static DynArr<ChkPos> pending_requests;
        for(auto it  = chunk_requests.cbegin(); it != chunk_requests.cend();) {
            if(client.sent_requests.count(*it) == 0) {
                LUX_LOG("requesting chunk {%zd, %zd, %zd}",
                        it->x, it->y, it->z);
                ChkPos& request = pending_requests.emplace_back(*it);
                client.sent_requests.emplace(request);
                LUX_LOG("requesting chunk {%zd, %zd, %zd}",
                        request.x, request.y, request.z);
                it = chunk_requests.erase(it);
            } else ++it;
        }
        chunk_requests.clear();
        if(pending_requests.size() > 0) {
            SizeT pack_sz = sizeof(NetCsSgnl::Header) +
                sizeof(NetCsSgnl::MapRequest) + pending_requests.size() *
                sizeof(ChkPos);

            ENetPacket* out_pack;
            if(create_reliable_pack(out_pack, pack_sz) != LUX_OK) {
                return; //@CONSIDER LUX_FAIL
            }
            U8* iter = out_pack->data;
            serialize(&iter, (U8 const&)NetCsSgnl::MAP_REQUEST);
            serialize(&iter, (U32 const&)pending_requests.size());
            for(auto const& request : pending_requests) {
                serialize(&iter, request);
            }
            LUX_ASSERT(iter == out_pack->data + out_pack->dataLength);
            if(send_packet(client.peer, out_pack, SIGNAL_CHANNEL) != LUX_OK) {
                return;
            }
            pending_requests.clear();
        }
    }

    ///send tick
    {
        ENetPacket* out_pack;
        if(create_unreliable_pack(out_pack, sizeof(NetCsTick)) != LUX_OK) {
            return;
        }
        U8* iter = out_pack->data;
        Vec2F dir = {0.f, 0.f};
        ///we ignore the input if console is opened up
        if(!console_is_active()) {
            if(glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS) {
                dir.y = -1.f;
            } else if(glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS) {
                dir.y = 1.f;
            }
            if(glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS) {
                dir.x = -1.f;
            } else if(glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS) {
                dir.x = 1.f;
            }
            if(dir.x != 0.f || dir.y != 0.f) {
                dir = glm::normalize(dir);
                dir = glm::rotate(dir, get_aim_rotation());
            }
        }
        serialize(&iter, dir);
        LUX_ASSERT(iter == out_pack->data + out_pack->dataLength);
        (void)send_packet(client.peer, out_pack, TICK_CHANNEL);
    }

    { ///IO
        glfwPollEvents();
        client.should_close |= glfwWindowShouldClose(glfw_window);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        map_render(player_pos);
        console_render();
        check_opengl_error();
        glfwSwapBuffers(glfw_window);
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

    //@TODO add logs there
    db_init();
    rendering_init();
    LUX_DEFER { rendering_deinit(); };
    map_init();
    {   Vec2<int> win_size;
        glfwGetWindowSize(glfw_window, &win_size.x, &win_size.y);
        console_init((Vec2U)win_size);
    }
    LUX_DEFER { console_deinit(); };
    glfwSetWindowSizeCallback(glfw_window, window_resize_cb);
    glfwSetKeyCallback(glfw_window, key_cb);
    check_opengl_error();

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
        std::memcpy(conf.name, client_name, sizeof(client_name));
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

    if(client.peer->state == ENET_PEER_STATE_CONNECTED) {
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
