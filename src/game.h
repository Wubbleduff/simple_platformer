
#pragma once

struct Game
{
    static struct GameState *instance;

    static void start();
    static void stop();

    static void add_player();
};


