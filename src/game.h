
#pragma once

struct Game
{
    static struct GameState *instance;

    static void start();
    static void stop();

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

        static PlayerID add();
        static void remove(PlayerID id);
        static bool action(PlayerID id, Action action);
    };

};


