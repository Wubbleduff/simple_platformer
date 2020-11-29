
#pragma once

#include "serialization.h"

struct Network
{
    static struct NetworkState *instance;

    enum class GameMode
    {
        CLIENT,
        SERVER,
    };

    static void init();

    // Client
    static void send_input_state_to_server();
    static void read_game_state(Serialization::Stream *game_state_stream);

    // Server
    static void read_client_input_states();
    static void broadcast_game_state(Serialization::Stream *game_state_stream);
};
