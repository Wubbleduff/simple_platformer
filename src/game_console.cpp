
#include "game_console.h"
#include "platform.h"
#include "data_structures.h"

#include "imgui.h"

#include <vector>
#include <cstring>
#include <cassert>



using namespace GameMath;



struct GameConsoleState
{
    struct Entry
    {
        std::vector<char> text;
        v3 color;
    };

    std::vector<Entry> entries;

};
GameConsoleState *GameConsole::instance = nullptr;



void GameConsole::init()
{
    instance = new GameConsoleState();
}

void GameConsole::write(const char *text, v3 color)
{
    GameConsoleState::Entry entry;
    entry.text.resize(strlen(text) + 1);

    entry.color = color;

    strcpy(entry.text.data(), text);

    instance->entries.push_back(entry);
}

void GameConsole::draw()
{
    ImGui::Begin("Console");

    for(int i = 0; i < instance->entries.size(); i++)
    {
        GameConsoleState::Entry *entry = &(instance->entries[i]);

        ImGui::TextColored(ImVec4(entry->color.r, entry->color.g, entry->color.b, 1.0f), entry->text.data());
    }

    ImGui::SetScrollHereY(1.0f);

    ImGui::End();
}

