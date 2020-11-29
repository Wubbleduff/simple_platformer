
#pragma once

struct Network
{
    static void read_client_input();
    static void broadcast_game_state(Serialization::Stream *game_state_stream);

    static void send_input_to_server();
    static void read_game_state(Serialization::Stream *game_state_stream);
};
