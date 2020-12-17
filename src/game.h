
#pragma once

#include "network.h"
#include <vector>

typedef int PlayerID;
struct Players
{
    enum class Action
    {
        MOVE_RIGHT,
        MOVE_LEFT,
        JUMP,

        NUM_ACTIONS
    };

    static bool action(PlayerID id, Action action);
    static PlayerID remote_to_local_player_id(PlayerID remote_id);
};

struct Game
{
    static struct GameState *instance;

    static void start();
    static void stop();
};


