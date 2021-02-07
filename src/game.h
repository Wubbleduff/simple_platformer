
#pragma once

#include "network.h"
#include "game_math.h"
#include <vector>



struct GameInput
{
    typedef unsigned int UID;
    UID uid;
    enum class Action
    {
        JUMP,
        SHOOT,

        NUM_ACTIONS
    };

    bool current_actions[(int)Action::NUM_ACTIONS] = {};
    float current_horizontal_movement; // Value between -1 and 1
    GameMath::v2 current_aiming_direction;

    bool action(Action action);

    void read_from_local();
    bool read_from_connection(Network::Connection **connection);
    void serialize(Serialization::Stream *stream, bool serialize);
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

