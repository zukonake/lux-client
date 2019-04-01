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
#include <imgui/imgui.h>
//
#include <lux_shared/common.hpp>
#include <lux_shared/net/common.hpp>
#include <lux_shared/net/data.hpp>
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
    //@TODO LUX_* defines
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
        if(client.host == nullptr) {
            LUX_LOG("couldn't initialize ENet host");
            return LUX_FAIL;
        }
    }
    net_compression_init(client.host);

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

    //@TODO abstract this out
    if(ui_has_rasen_label("entity_move"_l)) {
        Arr<U8, 2> stack;
        stack[0] = 1;
        stack[1] = 0;
        ui_add_continuous_binding("entity_move"_l, GLFW_KEY_W, stack);
        stack[0] = -1;
        ui_add_continuous_binding("entity_move"_l, GLFW_KEY_S, stack);
        stack[0] = 0;
        stack[1] = 1;
        ui_add_continuous_binding("entity_move"_l, GLFW_KEY_A, stack);
        stack[1] = -1;
        ui_add_continuous_binding("entity_move"_l, GLFW_KEY_D, stack);
    } else {
        LUX_LOG_WARN("no \"entity_move\" label loaded, couldn't create default "
            "key bindings");
    }
    /*if(ui_has_rasen_label("entity_rotate"_l)) {
        Arr<U8, 1> stack = {1};
        ui_add_continuous_binding("entity_rotate"_l, GLFW_KEY_D, stack);
        stack[0] = -1;
        ui_add_continuous_binding("entity_rotate"_l, GLFW_KEY_A, stack);
    } else {
        LUX_LOG_WARN("no \"entity_rotate\" label loaded, couldn't create "
            "default key bindings");
    }*/

    return LUX_OK;
}

LUX_MAY_FAIL static handle_tick(ENetPacket* in_pack) {
    LUX_RETHROW(deserialize_packet(in_pack, &ss_tick),
        "failed to deserialize tick");

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
            case NetSsSgnl::CHUNK_LOAD: {
                map_load_chunks(ss_sgnl.chunk_load);
                for(auto const& chunk : ss_sgnl.chunk_load.chunks) {
                    client.sent_requests.erase(chunk.first);
                }
            } break;
            case NetSsSgnl::CHUNK_UPDATE: {
                map_update_chunks(ss_sgnl.chunk_update);
            } break;
            case NetSsSgnl::MSG: {
                LUX_UNIMPLEMENTED();
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
    bool received_tick = false;
    { ///handle events
        if(glfwWindowShouldClose(glfw_window)) client_quit();
        ENetEvent event;
        //@TODO time
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
                    received_tick = true;
                } else if(event.channelID == SGNL_CHANNEL) {
                    LUX_RETHROW(handle_signal(event.packet),
                        "failed to handle signal from server");
                } else {
                    LUX_LOG("ignoring unexpected packet");
                    LUX_LOG("    channel: %u", event.channelID);
                    return LUX_FAIL;
                }
            }
        }
    }
    if(!received_tick) {
        LUX_LOG_WARN("lost server tick");
    }
    ///send map request signal
    {   cs_sgnl.tag = NetCsSgnl::MAP_REQUEST;
        for(auto const& pos : chunk_requests) {
            if(client.sent_requests.count(pos) == 0) {
                LUX_LOG("requesting chunk {%zd, %zd}", pos.x, pos.y);
                cs_sgnl.map_request.requests.emplace(pos);
                client.sent_requests.emplace(pos);
            }
        }
        chunk_requests.clear();
        if(cs_sgnl.map_request.requests.size() > 0) {
            LUX_RETHROW(send_net_data(client.peer, &cs_sgnl, SGNL_CHANNEL),
                "failed to send map requests");
        }
    }
    if(send_net_data(client.peer, &cs_tick, TICK_CHANNEL) != LUX_OK) {
        LUX_LOG("failed to send tick");
    }
    SizeT constexpr samples_num = 128;
    static U32 rx_sum = 0;
    static U32 tx_sum = 0;
    static U32 count  = 0;
    static U32 rx_max = 0;
    static U32 tx_max = 0;
    static U32 rx_avg = 0;
    static U32 tx_avg = 0;
    U32 rx = client.host->totalReceivedData;
    U32 tx = client.host->totalSentData;
    rx_max = max(rx_max, rx);
    tx_max = max(tx_max, tx);
    rx_sum += rx;
    tx_sum += rx;
    count++;
    if(count >= samples_num) {
        rx_avg = round((F64)rx_sum / (F64)count);
        tx_avg = round((F64)tx_sum / (F64)count);
        rx_sum = 0;
        tx_sum = 0;
        rx_max = 0;
        tx_max = 0;
        count  = 0;
    }
    ImGui::Begin("network status");
    ImGui::Text("(%zu tick avg.)", samples_num);
    ImGui::Text("tx: %uB", tx_avg);
    ImGui::Text("rx: %uB", rx_avg);
    ImGui::Text("(%zu tick max)", samples_num);
    ImGui::Text("tx: %uB", tx_max);
    ImGui::Text("rx: %uB", rx_max);
    ImGui::End();
    client.host->totalReceivedData = 0;
    client.host->totalSentData = 0;
    return LUX_OK;
}

LUX_MAY_FAIL client_send_assembly(Str const& label, Str const& contents) {
    cs_sgnl.tag = NetCsSgnl::RASEN_ASM;
    cs_sgnl.rasen_asm.str_id   = label;
    cs_sgnl.rasen_asm.contents = contents;
    return send_net_data(client.peer, &cs_sgnl, SGNL_CHANNEL);
}
