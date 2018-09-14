#include <cstdlib>
//
#include <enet/enet.h>
//
#include <lux/common.hpp>
#include <lux/net.hpp>

struct {
    Arr<U8, CLIENT_NAME_LEN> name = {0}; //@CONSIDER null character instead
} conf;

struct {
    ENetHost* host;
    ENetPeer* peer;

    ENetPacket*   reliable_out;
    ENetPacket* unreliable_out;

    String server_name;
} client;

void connect_to_server(char const* hostname, U16 port) {
    ENetAddress addr;
    if(enet_address_set_host(&addr, hostname) < 0) {
        LUX_FATAL("failed to set server hostname: %s", hostname);
    }
    addr.port = port;
    U8* ip = (U8*)&addr.host;
    static_assert(sizeof(addr.host) == 4);
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
        //@CONSIDER sharing code between server here
        #pragma pack(push, 1)
        struct {
            Arr<U8, 3> ver =
                {NET_VERSION_MAJOR, NET_VERSION_MINOR, NET_VERSION_PATCH};
            static_assert(sizeof(NET_VERSION_MAJOR) == sizeof(ver[0]));
            static_assert(sizeof(NET_VERSION_MINOR) == sizeof(ver[1]));
            static_assert(sizeof(NET_VERSION_PATCH) == sizeof(ver[2]));
            Arr<U8, CLIENT_NAME_LEN> name = conf.name;
        } client_init_data;
        #pragma pack(pop)

        client.reliable_out->data = (U8*)&client_init_data;
        client.reliable_out->dataLength = sizeof(client_init_data);
        if(enet_peer_send(client.peer, INIT_CHANNEL, client.reliable_out) < 0) {
            LUX_FATAL("failed to send init packet");
        }
        enet_host_flush(client.host);
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
        LUX_LOG("received init packet after %zu/%zu tries", tries, MAX_TRIES);

        #pragma pack(push, 1)
        struct { //@TODO tick
            Arr<U8, SERVER_NAME_LEN> name;
        } server_init_data;
        #pragma pack(pop)

        if(sizeof(server_init_data) != init_pack->dataLength) {
            LUX_FATAL("server sent invalid init packet with size %zu instead of"
                      " %zu", sizeof(server_init_data), init_pack->dataLength);
        }
        ///assumes no struct fields bigger than byte in init data
        std::memcpy((U8*)&server_init_data, init_pack->data,
                    sizeof(server_init_data));
        enet_packet_destroy(init_pack);
        client.server_name = String((char const*)server_init_data.name.data());
        LUX_LOG("successfully connected to server %s",
                client.server_name.c_str());
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

    { ///init client
        client.host = enet_host_create(nullptr, 1, CHANNEL_NUM, 0, 0);
        if(client.host == nullptr) {
            LUX_FATAL("couldn't initialize ENet host");
        }
        client.reliable_out = enet_packet_create(nullptr, 0,
            ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_NO_ALLOCATE);
        if(client.reliable_out == nullptr) {
            LUX_FATAL("couldn't initialize reliable output packet");
        }
        client.unreliable_out = enet_packet_create(nullptr, 0,
            ENET_PACKET_FLAG_UNSEQUENCED | ENET_PACKET_FLAG_NO_ALLOCATE);
        if(client.unreliable_out == nullptr) {
            LUX_FATAL("couldn't initialize unreliable output packet");
        }
    }

    { ///copy client name
        U8 constexpr client_name[] = "lux-client";
        static_assert(sizeof(client_name) <= CLIENT_NAME_LEN);
        std::memcpy(conf.name.data(), client_name, sizeof(client_name));
    }

    connect_to_server(server_hostname, server_port);

    enet_host_destroy(client.host);
    enet_deinitialize();
    return 0;
}
