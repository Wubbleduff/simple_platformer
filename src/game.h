
#pragma once

#include "network.h"

struct Game
{
    struct PlayerID
    {
        int id;
    };

    static struct GameState *instance;

    static void start();
    static void stop();

    struct Players
    {
        enum class Action
        {
            MOVE_RIGHT,
            MOVE_LEFT,
            JUMP,

            NUM_ACTIONS
        };

        static PlayerID add();
        static PlayerID add_remote(Network::Connection *Connection);
        static void remove(PlayerID id);
        static bool action(PlayerID id, Action action);
    };
};


