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
//
#include <map.hpp>
#include <console.hpp>
#include <rendering.hpp>
#include "client.hpp"

struct {
    ENetHost* host;
    ENetPeer* peer;

    String server_name  = "unnamed";
    bool   should_close = false;
    VecSet<ChkPos> sent_requests;
} client;

static void connect_to_server(char const* hostname, U16 port, F64& tick_rate);

void client_quit() {
    client.should_close = true;
}

bool client_should_close() {
    return client.should_close;
}

void client_init(char const* server_hostname, U16 server_port, F64& tick_rate) {
    LUX_LOG("initializing client");

    //@TODO add logs there
    if(enet_initialize() < 0) {
        LUX_FATAL("couldn't initialize ENet");
    }

    { ///init client
        client.host = enet_host_create(nullptr, 1, CHANNEL_NUM, 0, 0);
        if(client.host == nullptr) {
            LUX_FATAL("couldn't initialize ENet host");
        }
    }

    connect_to_server(server_hostname, server_port, tick_rate);
}

void client_deinit() {
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
    enet_host_destroy(client.host);
    enet_deinitialize();
}

static void connect_to_server(char const* hostname, U16 port, F64& tick_rate) {
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

        U8 constexpr client_name[] = "lux-client";
        static_assert(sizeof(client_name) <= CLIENT_NAME_LEN);
        Arr<char, CLIENT_NAME_LEN> buff;
        std::memcpy(buff, client_name, sizeof(client_name));
        std::memset(buff + sizeof(client_name), 0,
                    CLIENT_NAME_LEN - sizeof(client_name));
        serialize(&iter, buff);
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
            tick_rate = ss_init.tick_rate;
        }
        LUX_LOG("successfully connected to server %s",
                client.server_name.c_str());
        LUX_LOG("tick rate %2.f", tick_rate);
    }
}

LUX_MAY_FAIL static handle_tick(ENetPacket* in_pack, Vec3F& player_pos) {
    U8 const* iter = in_pack->data;
    if(check_pack_size(sizeof(NetSsTick), iter, in_pack) != LUX_OK) {
        LUX_LOG("failed to handle tick");
        return LUX_FAIL;
    }

    deserialize(&iter, &player_pos);
    LUX_ASSERT(iter == in_pack->data + in_pack->dataLength);
    return LUX_OK;
}

LUX_MAY_FAIL static handle_signal(ENetPacket* in_pack) {
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
        case NetSsSgnl::MSG: {
            expected_stt_sz = sizeof(NetSsSgnl::Msg);
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
        case NetSsSgnl::MSG: {
            deserialize(&iter, &sgnl.msg.contents.len);
            expected_dyn_sz = sgnl.msg.contents.len;
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
            case NetSsSgnl::MSG: {
                Slice<char> msg;
                msg.len = sgnl.msg.contents.len;
                msg.beg = lux_alloc<char>(msg.len);
                LUX_DEFER { lux_free(msg.beg); };
                for(Uns i = 0; i < msg.len; ++i) {
                    deserialize(&iter, &msg.beg[i]);
                }
                console_print(msg.beg);
            } break;
            default: LUX_UNREACHABLE();
        }
    }
    LUX_ASSERT(iter == in_pack->data + in_pack->dataLength);
    return LUX_OK;
}

//@TODO err handling
void client_tick(GLFWwindow* glfw_window, Vec3F& player_pos) {
    { ///handle events
        if(glfwWindowShouldClose(glfw_window)) client_quit();
        ENetEvent event;
        while(enet_host_service(client.host, &event, 0) > 0) {
            if(event.type == ENET_EVENT_TYPE_DISCONNECT) {
                LUX_LOG("connection closed by server");
                client.should_close = true;
                return;
            } else if(event.type == ENET_EVENT_TYPE_RECEIVE) {
                LUX_DEFER { enet_packet_destroy(event.packet); };
                if(event.channelID == TICK_CHANNEL) {
                    if(handle_tick(event.packet, player_pos) != LUX_OK) {
                        continue;
                    }
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
}

LUX_MAY_FAIL send_command(char const* beg) {
    char const* end = beg;
    while(*end != '\0') ++end;
    ///we want to count the null terminator
    ++end;
    SizeT len = end - beg;
    if(len > 0) {
        SizeT pack_sz = sizeof(NetCsSgnl::Header) +
            sizeof(NetCsSgnl::Command) + len;

        ENetPacket* out_pack;
        if(create_reliable_pack(out_pack, pack_sz) != LUX_OK) {
            LUX_LOG("failed to create packet for command\n%s", beg);
            return LUX_FAIL;
        }
        U8* iter = out_pack->data;
        serialize(&iter, (U8 const&)NetCsSgnl::COMMAND);
        serialize(&iter, (U32 const&)len);
        for(char const* i = beg; i < end; ++i) {
            serialize(&iter, *i);
        }
        LUX_ASSERT(iter == out_pack->data + out_pack->dataLength);
        if(send_packet(client.peer, out_pack, SIGNAL_CHANNEL) != LUX_OK) {
            LUX_LOG("failed to send packet for command\n%s", beg);
            return LUX_FAIL;
        }
    }
    return LUX_OK;
}
