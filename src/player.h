
#pragma once

typedef int PlayerID;
struct Players
{
    struct Player;
    enum class Action
    {
        MOVE_RIGHT,
        MOVE_LEFT,
        JUMP,
    };

    static bool action(PlayerID id, Action action);

    static void read_actions(Player *player);
};

