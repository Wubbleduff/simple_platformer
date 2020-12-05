
#pragma once

#include "serialization.h"

struct Network
{
    static struct NetworkState *instance;

    enum class GameMode
    {
        OFFLINE,
        CLIENT,
        SERVER,
    };

    static void init();

    // Client
    struct Client
    {
        static void init();
        static void connect_to_server(const char *ip_address, int port);
        static void send_input_state_to_server();
        static bool read_game_state(Serialization::Stream *game_state_stream);
    };

    // Server
    struct Server
    {
        static void listen_for_client_connections(int port);
        static void accept_client_connections();
        static bool read_client_input_states();
        static void broadcast_game(Serialization::Stream *game_stream);
    };
};

