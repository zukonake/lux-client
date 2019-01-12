#if defined(__unix__)
    #include <unistd.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif
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
#include <rendering.hpp>
#include <entity.hpp>
#include "client.hpp"

struct {
    ENetHost* host;
    ENetPeer* peer;

    StrBuff server_name  = "unnamed"_l;
    bool    should_close = false;
    VecSet<ChkPos> sent_requests;
} static client;

EntityVec last_player_pos = {0, 0, 0};
F64 tick_rate = 0.f;

NetCsTick cs_tick;
NetCsSgnl cs_sgnl;
NetSsTick ss_tick;
NetSsSgnl ss_sgnl;

LUX_MAY_FAIL static connect_to_server(char const* hostname, U16 port);

void client_quit() {
    client.should_close = true;
}

bool client_should_close() {
    return client.should_close;
}

static void get_user_name(char* buff) {
    constexpr char unknown[] = "unknown";
    static_assert(sizeof(unknown) - 1 <= CLIENT_NAME_LEN);
    std::memset(buff, 0, CLIENT_NAME_LEN);
#if defined(__unix__)
    if(getlogin_r((char*)buff, CLIENT_NAME_LEN) != 0) {
        std::memcpy(buff, unknown, sizeof(unknown) - 1);
    }
#elif defined(_WIN32)
    LPDWORD sz = CLIENT_NAME_LEN;
    if(GetUserName(buff, &sz) == 0) {
        std::memcpy(buff, unknown, sizeof(unknown) - 1);
    }
#else
    std::memcpy(buff, unknown, sizeof(unknown) - 1);
#endif
}

LUX_MAY_FAIL client_init(char const* server_hostname, U16 server_port) {
    LUX_LOG("initializing client");

    if(enet_initialize() < 0) {
        LUX_LOG("couldn't initialize ENet");
        return LUX_FAIL;
    }

    { ///init client
        client.host = enet_host_create(nullptr, 1, CHANNEL_NUM, 0, 0);
        //@TODO enet_host_compress_with_range_coder(client.host);
        if(client.host == nullptr) {
            LUX_LOG("couldn't initialize ENet host");
            return LUX_FAIL;
        }
    }

    return connect_to_server(server_hostname, server_port);
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

LUX_MAY_FAIL static connect_to_server(char const* hostname, U16 port) {
    ENetAddress addr;
    if(enet_address_set_host(&addr, hostname) < 0) {
        LUX_LOG("failed to set server hostname: %s", hostname);
        return LUX_FAIL;
    }
    addr.port = port;
    U8* ip = get_ip(addr);
    LUX_LOG("connecting to %u.%u.%u.%u:%u",
            ip[0], ip[1], ip[2], ip[3], port);
    client.peer = enet_host_connect(client.host, &addr, CHANNEL_NUM, 0);
    if(client.peer == nullptr) {
        LUX_LOG("failed to connect to server");
        return LUX_FAIL;
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
                LUX_LOG("failed to acknowledge connection with server");
                return LUX_FAIL;
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
        get_user_name(cs_init.name);
        LUX_RETHROW(send_net_data(client.peer, &cs_init, INIT_CHANNEL),
            "failed to send init packet");
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
            LUX_RETHROW(deserialize_packet(in_pack, &ss_init),
                "failed to receive init packet");

            client.server_name = (Str)ss_init.name;
            tick_rate = ss_init.tick_rate;
            for(auto const& label : ss_init.rasen_labels) {
                LUX_RETHROW(ui_add_rasen_label(label));
            }
        }
        LUX_LOG("successfully connected to server \"%.*s\"",
                (int)client.server_name.len, client.server_name.beg);
        LUX_LOG("tick rate %2.f", tick_rate);
    }

    if(ui_has_rasen_label("entity_move"_l)) {
        Arr<U8, 2> stack = {1, 0};
        ui_add_continuous_binding("entity_move"_l, GLFW_KEY_W, stack);
        stack[0] = -1;
        ui_add_continuous_binding("entity_move"_l, GLFW_KEY_S, stack);
    } else {
        LUX_LOG_WARN("no \"entity_move\" label loaded, couldn't create default "
            "key bindings");
    }
    if(ui_has_rasen_label("entity_rotate"_l)) {
        Arr<U8, 1> stack = {1};
        ui_add_continuous_binding("entity_rotate"_l, GLFW_KEY_D, stack);
        stack[0] = -1;
        ui_add_continuous_binding("entity_rotate"_l, GLFW_KEY_A, stack);
    } else {
        LUX_LOG_WARN("no \"entity_rotate\" label loaded, couldn't create "
            "default key bindings");
    }

    return LUX_OK;
}

LUX_MAY_FAIL static handle_tick(ENetPacket* in_pack) {
    LUX_RETHROW(deserialize_packet(in_pack, &ss_tick),
        "failed to deserialize tick");

    //@TODO if we have no position, we should not render the map
    if(ss_tick.entity_comps.pos.count(ss_tick.player_id) > 0) {
        last_player_pos = ss_tick.entity_comps.pos.at(ss_tick.player_id);
    }
    entities.clear();
    for(auto const& id : ss_tick.entities) {
        entities.emplace(id);
    }
    set_net_entity_comps(ss_tick.entity_comps);
    return LUX_OK;
}

LUX_MAY_FAIL static handle_signal(ENetPacket* in_pack) {
    LUX_RETHROW(deserialize_packet(in_pack, &ss_sgnl),
        "failed to deserialize signal");

    { ///parse the packet
        switch(ss_sgnl.tag) {
            case NetSsSgnl::BLOCKS: {
                for(auto const& chunk : ss_sgnl.blocks.chunks) {
                    blocks_update(chunk.first, chunk.second);
                }
            } break;
            case NetSsSgnl::LIGHT: {
                for(auto const& chunk : ss_sgnl.light.chunks) {
                    light_update(chunk.first, chunk.second);
                }
            } break;
            case NetSsSgnl::MSG: {
                //@TODO console_print(ss_sgnl.msg.contents.data());
            } break;
            case NetSsSgnl::RASEN_LABEL: {
                ui_add_rasen_label(ss_sgnl.rasen_label);
            } break;
            default: LUX_UNREACHABLE();
        }
    }
    return LUX_OK;
}

LUX_MAY_FAIL client_tick(GLFWwindow* glfw_window) {
    { ///handle events
        if(glfwWindowShouldClose(glfw_window)) client_quit();
        ENetEvent event;
        while(enet_host_service(client.host, &event, 0) > 0) {
            if(event.type == ENET_EVENT_TYPE_DISCONNECT) {
                LUX_LOG("connection closed by server");
                client.should_close = true;
                return LUX_OK;
            } else if(event.type == ENET_EVENT_TYPE_RECEIVE) {
                LUX_DEFER { enet_packet_destroy(event.packet); };
                if(event.channelID == TICK_CHANNEL) {
                    if(handle_tick(event.packet) != LUX_OK) {
                        LUX_LOG("ignoring tick");
                        continue;
                    }
                } else if(event.channelID == SIGNAL_CHANNEL) {
                    LUX_RETHROW(handle_signal(event.packet),
                        "failed to handle signal from server");
                } else {
                    LUX_LOG("ignoring unexpected packet");
                    LUX_LOG("    channel: %u", event.channelID);
                }
            }
        }
    }
    ///send map request signal
    {   cs_sgnl.tag = NetCsSgnl::MAP_REQUEST;
        for(auto it  = chunk_requests.cbegin(); it != chunk_requests.cend();) {
            if(client.sent_requests.count(*it) == 0) {
                LUX_LOG("requesting chunk {%zd, %zd}", it->x, it->y);
                cs_sgnl.map_request.requests.emplace(*it);
                client.sent_requests.emplace(*it);
                it = chunk_requests.erase(it);
            } else ++it;
        }
        chunk_requests.clear();
        if(cs_sgnl.map_request.requests.size() > 0) {
            if(send_net_data(client.peer, &cs_sgnl, SIGNAL_CHANNEL) != LUX_OK) {
                LUX_LOG("failed to send map requests");
            }
        }
    }

    //@TODO if(!console_is_active()) {
        enum {
            NO_DIR,
            FORWARD,
            BACKWARD,
        } dir = NO_DIR;
        enum {
            NO_ANGLE,
            RIGHT,
            LEFT,
        } angle = NO_ANGLE;
        if(glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS) {
            dir = FORWARD;
        } else if(glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS) {
            dir = BACKWARD;
        }
        if(glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS) {
            angle = LEFT;
        } else if(glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS) {
            angle = RIGHT;
        }
        /*
        if(dir != NO_DIR) {
            auto& action = cs_tick.actions.emplace();
            U8 forward = dir == FORWARD;
            action.bytecode = {
                RN_PUSHV(forward),
                RN_LOADV(0xff, RN_R0),
                RN_PUSH (RN_R0),
                RN_XCALL(RN_XC_ENTITY_MOVE),
                RN_XCALL(RN_XC_HALT),
            };
        }
        if(angle != NO_ANGLE) {
            auto& action = cs_tick.actions.emplace();
            U8 right = angle == RIGHT;
            action.bytecode = {
                RN_PUSHV(right),
                RN_LOADV(0xff, RN_R0),
                RN_PUSH (RN_R0),
                RN_XCALL(RN_XC_ENTITY_ROTATE),
                RN_XCALL(RN_XC_HALT),
            };
        }*/
    //}
    if(send_net_data(client.peer, &cs_tick, TICK_CHANNEL) != LUX_OK) {
        LUX_LOG("failed to send tick");
    }
    return LUX_OK;
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
        LUX_RETHROW(send_net_data(client.peer, &cs_sgnl, SIGNAL_CHANNEL),
            "failed to send command");
    }
    return LUX_OK;
}

LUX_MAY_FAIL client_send_assembly(Str const& label, Str const& contents) {
    cs_sgnl.tag = NetCsSgnl::RASEN_ASM;
    cs_sgnl.rasen_asm.str_id   = label;
    cs_sgnl.rasen_asm.contents = contents;
    return send_net_data(client.peer, &cs_sgnl, SIGNAL_CHANNEL);
}

void add_dbg_point(NetSsTick::DbgInf::Shape::Point const& val,
                   Vec4F col, bool border) {
    ss_tick.dbg_inf.shapes.emplace(NetSsTick::DbgInf::Shape{
        .tag = NetSsTick::DbgInf::Shape::POINT, .point = val,
        .col = col, .border = border});
}

void add_dbg_line(NetSsTick::DbgInf::Shape::Line const& val,
                   Vec4F col, bool border) {
    ss_tick.dbg_inf.shapes.emplace(NetSsTick::DbgInf::Shape{
        .tag = NetSsTick::DbgInf::Shape::LINE, .line = val,
        .col = col, .border = border});
}

void add_dbg_arrow(NetSsTick::DbgInf::Shape::Arrow const& val,
                   Vec4F col, bool border) {
    ss_tick.dbg_inf.shapes.emplace(NetSsTick::DbgInf::Shape{
        .tag = NetSsTick::DbgInf::Shape::ARROW, .arrow = val,
        .col = col, .border = border});
}

void add_dbg_cross(NetSsTick::DbgInf::Shape::Cross const& val,
                   Vec4F col, bool border) {
    ss_tick.dbg_inf.shapes.emplace(NetSsTick::DbgInf::Shape{
        .tag = NetSsTick::DbgInf::Shape::CROSS, .cross = val,
        .col = col, .border = border});
}

void add_dbg_sphere(NetSsTick::DbgInf::Shape::Sphere const& val,
                   Vec4F col, bool border) {
    ss_tick.dbg_inf.shapes.emplace(NetSsTick::DbgInf::Shape{
        .tag = NetSsTick::DbgInf::Shape::SPHERE, .sphere = val,
        .col = col, .border = border});
}

void add_dbg_triangle(NetSsTick::DbgInf::Shape::Triangle const& val,
                   Vec4F col, bool border) {
    ss_tick.dbg_inf.shapes.emplace(NetSsTick::DbgInf::Shape{
        .tag = NetSsTick::DbgInf::Shape::TRIANGLE, .triangle = val,
        .col = col, .border = border});
}

void add_dbg_rect(NetSsTick::DbgInf::Shape::Rect const& val,
                   Vec4F col, bool border) {
    ss_tick.dbg_inf.shapes.emplace(NetSsTick::DbgInf::Shape{
        .tag = NetSsTick::DbgInf::Shape::RECT, .rect = val,
        .col = col, .border = border});
}
