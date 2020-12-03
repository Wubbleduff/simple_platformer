
#include "game_console.h"
#include "platform.h"

#include "imgui.h"

#include <cstring>
#include <cassert>



struct GameConsoleState
{
    static const int SIZE = 1024 * 1024 * 4;
    char *text_buffer;
    char *current;
};
GameConsoleState *GameConsole::instance = nullptr;



void GameConsole::init()
{
    instance = (GameConsoleState *)Platform::Memory::allocate(sizeof(GameConsoleState));

    instance->text_buffer = (char *)Platform::Memory::allocate(GameConsoleState::SIZE);
    instance->current = instance->text_buffer;
}

void GameConsole::write(const char *text)
{
    int len = strlen(text);
    strcpy(instance->current, text);
    instance->current += len;

    assert(instance->current < instance->text_buffer + 1024 * 1024 * 4);
}

void GameConsole::draw()
{
    ImGui::Begin("Console");

    ImGui::Text(instance->text_buffer);

    ImGui::SetScrollHereY(1.0f);

    ImGui::End();
}

