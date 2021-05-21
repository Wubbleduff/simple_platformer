
#include "levels.h"
#include "platform.h"
#include "logging.h"
#include "game_math.h"
#include "algorithms.h"
#include "data_structures.h"
#include "graphics.h"
#include "serialization.h"

#include "imgui.h"
#include <map>
#include <algorithm>
#include <cassert>



using namespace GameMath;



#define LEVELS_DIR "assets/data/levels/"
struct LevelsState
{
    Level *active_level = nullptr;

    bool lobby_level_selecting = false;

    typedef std::map<int, char *> LevelFilesMap;
    LevelFilesMap level_files =
    {
        {0, LEVELS_DIR "lobby"},
        {1, LEVELS_DIR "level_1"},
        {2, LEVELS_DIR "level_2"}
    };
};
LevelsState *Levels::instance = nullptr;









#pragma region Grid

void Level::Grid::Cell::reset()
{
    filled = false;
    win_when_touched = false;
}

void Level::Grid::init()
{
}

void Level::Grid::clear()
{
    cells_map.clear();
}

Level::Grid::Cell *Level::Grid::at(v2i pos)
{
    std::map<v2i, Cell *>::iterator it = cells_map.find(pos);
    if(it == cells_map.end())
    {
        cells_map[pos] = new Cell();
        return cells_map[pos];
    }
    else
    {
        return it->second;
    }
}

v2 Level::Grid::cell_to_world(v2i pos)
{
    v2 world_pos = v2((float)pos.x, (float)pos.y);
    return world_pos * world_scale;
}

v2i Level::Grid::world_to_cell(v2 pos)
{
    pos *= (1.0f / world_scale);
    v2i cell = v2i((int)pos.x, (int)pos.y); // Should return the bottom left of a grid cell in the world
    if(pos.x < 0.0f) cell.x -= 1;
    if(pos.y < 0.0f) cell.y -= 1;
    return cell;
}

void Level::Grid::serialize(Serialization::Stream *stream, bool writing)
{
    if(writing)
    {
        stream->write(world_scale);
        stream->write(start_cell.x);
        stream->write(start_cell.y);
        stream->write((int)cells_map.size());
        for(auto &pair : cells_map)
        {
            v2i pos = v2i(pair.first.x, pair.first.y);
            const Cell *cell = pair.second;

            stream->write(pos.x);
            stream->write(pos.y);
            stream->write(cell->filled ? 1 : 0);
            stream->write(cell->win_when_touched ? 1 : 0);
        }
    }
    else
    {
        stream->read(&world_scale);
        stream->read(&start_cell.x);
        stream->read(&start_cell.y);
        int size;
        stream->read(&size);
        for(int i = 0; i < size; i++)
        {
            v2i pos;
            stream->read(&pos.x);
            stream->read(&pos.y);

            int filled_val;
            stream->read(&filled_val);
            int win_when_touched;
            stream->read(&win_when_touched);

            bool filled = filled_val == 0 ? false : true;
            at(pos)->filled = filled;
        }
    }

}

#pragma endregion







void Level::Editor::step(Level *level, float time_step)
{
    int x;
    int y;
    Platform::Input::mouse_screen_position(&x, &y);
    v2 mouse_pos = v2(x, y);

    // Move camera
    if(Platform::Input::mouse_button(1) && !ImGui::IsAnyWindowHovered())
    {
        v2 mouse_delta = mouse_pos - last_mouse_position;
        mouse_delta.y *= -1.0f;
        camera_position += -mouse_delta * 0.05f;
    }
    last_mouse_position = mouse_pos;
    Graphics::Camera::position() = camera_position;

    // Set start point
    if(placing_start_point && !ImGui::IsAnyWindowHovered())
    {
        if(Platform::Input::mouse_button_down(0))
        {
            v2 pos = Platform::Input::mouse_world_position();
            v2i grid_pos = level->grid.world_to_cell(pos);
            level->grid.start_cell = grid_pos;
        }
    }
    Graphics::quad(level->grid.cell_to_world(level->grid.start_cell), v2(1.0f, 1.0f), 0.0f, v4(0.0f, 1.0f, 0.0f, 1.0f));

    // Place terrain
    if(placing_terrain && !ImGui::IsAnyWindowHovered())
    {
        if(Platform::Input::mouse_button(0))
        {
            v2 pos = Platform::Input::mouse_world_position();
            v2i grid_pos = level->grid.world_to_cell(pos);
            level->grid.at(grid_pos)->filled = true;
        }

        if(Platform::Input::key(Platform::Input::Key::SHIFT) && Platform::Input::mouse_button(0))
        {
            v2 pos = Platform::Input::mouse_world_position();
            v2i grid_pos = level->grid.world_to_cell(pos);
            level->grid.at(grid_pos)->filled = false;
        }
    }

    if(placing_win_points && !ImGui::IsAnyWindowHovered())
    {
        if(Platform::Input::mouse_button(0))
        {
            v2 pos = Platform::Input::mouse_world_position();
            v2i grid_pos = level->grid.world_to_cell(pos);
            level->grid.at(grid_pos)->win_when_touched = true;
        }

        if(Platform::Input::key(Platform::Input::Key::SHIFT) && Platform::Input::mouse_button(0))
        {
            v2 pos = Platform::Input::mouse_world_position();
            v2i grid_pos = level->grid.world_to_cell(pos);
            level->grid.at(grid_pos)->win_when_touched = false;
        }
    }


    if(Platform::Input::key_down(Platform::Input::Key::ESC))
    {
        Engine::switch_game_mode(GameState::MAIN_MENU);
    }
}

void Level::Editor::draw(Level *level)
{
    ImGui::Begin("Editor");

    draw_file_menu(level);

    if(ImGui::Button("Place terrain"))
    {
        placing_terrain = !placing_terrain;
    }

    const char* listbox_items[] = { "Start point", "Terrain", "Win points" };
    static int listbox_item_current = 1;
    ImGui::ListBox("Modes", &listbox_item_current, listbox_items, IM_ARRAYSIZE(listbox_items), 4);
    for(int i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) modes[i] = false;
    modes[listbox_item_current] = true;

    ImGui::ShowDemoWindow();

    ImGui::End();
}

void Level::Editor::draw_file_menu(Level *level)
{
    if(ImGui::Button("File"))
    {
        ImGui::OpenPopup("file_menu");
    }
    if(ImGui::BeginPopup("file_menu"))
    {
        ImGui::MenuItem(loaded_level, NULL, false, false);
        if(ImGui::Button("New"))
        {
            ImGui::OpenPopup("confirmation");
        }
        if(ImGui::BeginPopup("confirmation"))
        {
            ImGui::Text("Are you sure?");
            if(ImGui::MenuItem("Yes"))
            {
                level->init_default_level();
            }
            if(ImGui::MenuItem("No"))
            {
            }
            ImGui::EndPopup();
        }
        bool popup_opened_this_step = false;
        if(ImGui::Button("Open"))
        {
            ImGui::OpenPopup("open_level");
            Platform::Memory::memset(text_input_buffer, 0, sizeof(text_input_buffer));
            popup_opened_this_step = true;
        }
        static const char *LEVEL_PATH = "assets/data/levels/";
        if(ImGui::BeginPopup("open_level"))
        {
            if(popup_opened_this_step) ImGui::SetKeyboardFocusHere(0);
            if(ImGui::InputText("Level name", text_input_buffer,
                        sizeof(text_input_buffer) - sizeof(LEVEL_PATH) - 1,
                        ImGuiInputTextFlags_EnterReturnsTrue))
            {
                if(strlen(text_input_buffer) > 0)
                {
                    level->clear();
                    strcpy(temp_input_buffer, LEVEL_PATH);
                    strcat(temp_input_buffer, text_input_buffer);
                    level->load_with_file(temp_input_buffer, false);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        /*
        if(ImGui::BeginMenu("Open Recent"))
        {
            ImGui::MenuItem("fish_hat.c");
            ImGui::MenuItem("fish_hat.inl");
            ImGui::MenuItem("fish_hat.h");
            if(ImGui::BeginMenu("More.."))
            {
                ImGui::MenuItem("Hello");
                ImGui::MenuItem("Sailor");
                if(ImGui::BeginMenu("Recurse.."))
                {
                    //ShowExampleMenuFile();
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        */
        /*
        if(ImGui::Button("Save", "Ctrl+S"))
        {
        }
        */
        if(ImGui::Button("Save As.."))
        {
            ImGui::OpenPopup("save_level");
            popup_opened_this_step = true;
        }
        if(ImGui::BeginPopup("save_level"))
        {
            if(popup_opened_this_step) ImGui::SetKeyboardFocusHere(0);
            if(ImGui::InputText("Level name", text_input_buffer,
                        sizeof(text_input_buffer) - sizeof(LEVEL_PATH) - 1,
                        ImGuiInputTextFlags_EnterReturnsTrue))
            {
                if(strlen(text_input_buffer) > 0)
                {
                    strcpy(temp_input_buffer, LEVEL_PATH);
                    strcat(temp_input_buffer, text_input_buffer);
                    level->load_with_file(temp_input_buffer, true);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }


        ImGui::EndPopup();
    }
}







void Level::clear()
{
    number = -1;

    grid.init();
    grid.world_scale = 1.0f;
    grid.clear();
}

void Level::init(int level_num)
{
    clear();

    LevelsState::LevelFilesMap::iterator it = Levels::instance->level_files.find(level_num);
    if(it != Levels::instance->level_files.end())
    {
        char *path = it->second;
        load_with_file(path, false);
        number = level_num;
    }
    else
    {
        Log::log_error("Could not find file associated with level %i", level_num);
        init_default_level();
    }
}

void Level::init_default_level()
{
    clear();

    for(v2i pos = {-10, 0}; pos.x <= 10; pos.x++)
    {
        grid.at(pos)->filled = true;
    }

    strcpy(editor.loaded_level, "(empty)");
}

void Level::uninit()
{
}

void Level::step(float time_step)
{

}

void Level::draw()
{
    // Draw grid terrain
    for(auto &pair : grid.cells_map)
    {
        v2i pos = v2i(pair.first.x, pair.first.y);
        Grid::Cell *cell = pair.second;
        if(cell->filled)
        {
            float inten = (float)pos.x / 100.0f;
            v4 color = v4(1.0f - inten, 0.0f, inten, 1.0f);
            if(pos == v2i(0, 0)) color = v4(1.0f, 1.0f, 1.0f, 1.0f);

            v2 world_pos = grid.cell_to_world(pos) + v2(1.0f, 1.0f) * grid.world_scale / 2.0f;
            float scale = grid.world_scale * 0.95f;
            Graphics::quad(world_pos, v2(scale, scale), 0.0f, color);
        }

        if(cell->win_when_touched)
        {
            v4 color = v4(0.0f, 1.0f, 0.0f, 1.0f);
            v2 world_pos = grid.cell_to_world(pos) + v2(1.0f, 1.0f) * grid.world_scale / 2.0f;
            float scale = grid.world_scale * 0.95f;
            Graphics::quad(world_pos, v2(scale, scale), 0.0f, color);
        }
    }
}

void Level::serialize(Serialization::Stream *stream, bool writing)
{
    if(writing)
    {
        stream->write(number);
        grid.serialize(stream, true);
    }
    else
    {
        stream->read(&number);
        grid.serialize(stream, false);
    }
}

void Level::load_with_file(const char *path, bool writing)
{
    if(writing)
    {
        Serialization::Stream *stream = Serialization::make_stream();
        serialize(stream, true);
        stream->write_to_file(path);
        Serialization::free_stream(stream);
    }
    else
    {
        Serialization::Stream *stream = Serialization::make_stream_from_file(path);
        if(stream)
        {
            serialize(stream, false);
            Serialization::free_stream(stream);

            const char *name = strrchr(path, '/');
            if(name != nullptr)
            {
                strcpy(editor.loaded_level, name + 1);
            }
        }
        else
        {
            init_default_level();
            strcpy(editor.loaded_level, "(empty)");
        }
    }
}



void Levels::init()
{
    instance = new LevelsState();

    instance->active_level = create_level(0);
}

Level *Levels::create_level(int level_num)
{
    Level *new_level = new Level();
    new_level->init(level_num);
    return new_level;
}

void Levels::destroy_level(Level *level)
{
    level->uninit();
    delete level;
}



