#include <config.hpp>
//
#include <cstring>
//
#include <enet/enet.h>
#include <glm/gtx/rotate_vector.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/net/common.hpp>
#include <lux_shared/net/serial.hpp>
#include <lux_shared/net/data.hpp>
#include <lux_shared/net/data.inl>
#include <lux_shared/net/enet.hpp>
//
#include <map.hpp>
#include <console.hpp>
#include <rendering.hpp>
#include <entity.hpp>
#include "client.hpp"

struct {
    ENetHost* host;
    ENetPeer* peer;

    DynStr server_name  = "unnamed";
    bool   should_close = false;
    VecSet<ChkPos> sent_requests;
    EntityVec last_player_pos = {0, 0, 0};
} static client;

static NetCsTick cs_tick;
static NetCsSgnl cs_sgnl;
static NetSsTick ss_tick;
static NetSsSgnl ss_sgnl;

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
        NetCsInit cs_init;
        cs_init.net_ver.major = NET_VERSION_MAJOR;
        cs_init.net_ver.minor = NET_VERSION_MINOR;
        cs_init.net_ver.patch = NET_VERSION_PATCH;
        U8 constexpr client_name[] = "lux-client";
        static_assert(sizeof(client_name) <= CLIENT_NAME_LEN);
        std::memcpy(cs_init.name, client_name, sizeof(client_name));
        std::memset(cs_init.name + sizeof(client_name), 0,
                    CLIENT_NAME_LEN - sizeof(client_name));
        if(send_net_data(client.peer, &cs_init, INIT_CHANNEL) != LUX_OK) {
            //@TODO return
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
            if(deserialize_packet(in_pack, &ss_init) != LUX_OK) {
                LUX_FATAL("failed to receive init packet");
            }

            client.server_name = DynStr((char const*)ss_init.name);
            tick_rate = ss_init.tick_rate;
        }
        LUX_LOG("successfully connected to server %s",
                client.server_name.c_str());
        LUX_LOG("tick rate %2.f", tick_rate);
    }
}

LUX_MAY_FAIL static handle_tick(ENetPacket* in_pack) {
    if(deserialize_packet(in_pack, &ss_tick) != LUX_OK) {
        LUX_LOG("deserialization failed");
        return LUX_FAIL;
    }

    if(ss_tick.comps.pos.count(ss_tick.player_id) > 0) {
        client.last_player_pos = ss_tick.comps.pos.at(ss_tick.player_id);
    }
    return LUX_OK;
}

LUX_MAY_FAIL static handle_signal(ENetPacket* in_pack) {
    if(deserialize_packet(in_pack, &ss_sgnl) != LUX_OK) {
        return LUX_FAIL;
    }

    { ///parse the packet
        switch(ss_sgnl.tag) {
            case NetSsSgnl::MAP_LOAD: {
                for(auto const& chunk : ss_sgnl.map_load.chunks) {
                    load_chunk(chunk.first, chunk.second);
                }
            } break;
            case NetSsSgnl::LIGHT_UPDATE: {
                for(auto const& chunk : ss_sgnl.light_update.chunks) {
                    light_update(chunk.first, chunk.second);
                }
            } break;
            case NetSsSgnl::MSG: {
                console_print(ss_sgnl.msg.contents.data());
            } break;
            default: LUX_UNREACHABLE();
        }
    }
    return LUX_OK;
}

//@TODO err handling
void client_tick(GLFWwindow* glfw_window) {
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
                    if(handle_tick(event.packet) != LUX_OK) {
                        LUX_LOG("ignoring tick");
                        continue;
                    }
                } else if(event.channelID == SIGNAL_CHANNEL) {
                    if(handle_signal(event.packet) != LUX_OK) {
                        //@TODO consider crashing here
                        LUX_LOG("ignoring signal");
                        continue;
                    }
                } else {
                    LUX_LOG("ignoring unexpected packet");
                    LUX_LOG("    channel: %u", event.channelID);
                }
            }
        }
    }
    map_render(client.last_player_pos);
    entity_render(client.last_player_pos, ss_tick.comps);

    ///send map request signal
    {   cs_sgnl.tag = NetCsSgnl::MAP_REQUEST;
        for(auto it  = chunk_requests.cbegin(); it != chunk_requests.cend();) {
            if(client.sent_requests.count(*it) == 0) {
                LUX_LOG("requesting chunk {%zd, %zd, %zd}",
                        it->x, it->y, it->z);
                cs_sgnl.map_request.requests.emplace(*it);
                client.sent_requests.emplace(*it);
                it = chunk_requests.erase(it);
            } else ++it;
        }
        chunk_requests.clear();
        if(cs_sgnl.map_request.requests.size() > 0) {
            if(send_net_data(client.peer, &cs_sgnl, SIGNAL_CHANNEL) != LUX_OK) {
                //@TODO ?
                return;
            }
        }
    }

    ///send tick
    {   cs_tick.player_dir = {0.f, 0.f};
        if(!console_is_active()) {
            auto& dir = cs_tick.player_dir;
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
        (void)send_net_data(client.peer, &cs_tick, TICK_CHANNEL);
    }
}

LUX_MAY_FAIL send_command(char const* beg) {
    char const* end = beg;
    while(*end != '\0') ++end;
    ///we want to count the null terminator
    ++end;
    SizeT len = end - beg;
    if(len > 0) {
        cs_sgnl.tag = NetCsSgnl::COMMAND;
        cs_sgnl.command.contents.resize(len);
        for(Uns i = 0; i < len; ++i) {
            cs_sgnl.command.contents[i] = beg[i];
        }
        if(send_net_data(client.peer, &cs_sgnl, SIGNAL_CHANNEL) != LUX_OK) {
            return LUX_FAIL;
        }
    }
    return LUX_OK;
}
