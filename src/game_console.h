
#pragma once

struct GameConsole
{
    static struct GameConsoleState *instance;

    static void init();

    static void write(const char *text);

    static void draw();
};

