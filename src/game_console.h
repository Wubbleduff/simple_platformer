
#pragma once

#include "game_math.h" // v3

struct GameConsole
{
    static struct GameConsoleState *instance;

    static void init();

    static void write(const char *text, GameMath::v3 color = GameMath::v3(1.0f, 1.0f, 1.0f));

    static void draw();
};

