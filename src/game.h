
#pragma once

#include "network.h"
#include <vector>



struct GameInput
{
    typedef unsigned int UID;
    UID uid;
    enum class Action
    {
        MOVE_RIGHT,
        MOVE_LEFT,
        JUMP,

        NUM_ACTIONS
    };

    bool current_actions[(int)Action::NUM_ACTIONS] = {};

    bool action(Action action);
    void read_from_local();
    void read_from_connection(Network::Connection **connection);
    void serialize_into(Serialization::Stream *stream);
    void deserialize_from(Serialization::Stream *stream);
};
typedef std::vector<GameInput> GameInputList;

struct Game
{
    static struct ProgramState *program_state;

    static void start();
    static void stop();

    static GameInput::UID get_my_uid();

    static void exit_to_main_menu();
};

