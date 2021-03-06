
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






static bool aabb(v2 a_bl, v2 a_tr, v2 b_bl, v2 b_tr, v2 *dir, float *depth)
{
    float a_left   = a_bl.x;
    float a_right  = a_tr.x;
    float a_top    = a_tr.y;
    float a_bottom = a_bl.y;

    float b_left   = b_bl.x;
    float b_right  = b_tr.x;
    float b_top    = b_tr.y;
    float b_bottom = b_bl.y;

    float left_diff   = a_left - b_right;
    float right_diff  = b_left - a_right;
    float top_diff    = b_bottom - a_top;
    float bottom_diff = a_bottom - b_top;

    float max_depth = max(left_diff, max(right_diff, max(top_diff, bottom_diff)));

    if(max_depth > 0.0f)
    {
        return false;
    }

    if(max_depth == left_diff)   { *dir = v2(-1.0f,  0.0f); }
    if(max_depth == right_diff)  { *dir = v2( 1.0f,  0.0f); }
    if(max_depth == top_diff)    { *dir = v2( 0.0f,  1.0f); }
    if(max_depth == bottom_diff) { *dir = v2( 0.0f, -1.0f); }

    *depth = -max_depth;

    return true;
}


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

Level::v2i Level::Grid::world_to_cell(v2 pos)
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
        stream->write(start_point.x);
        stream->write(start_point.y);
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
        stream->read(&start_point.x);
        stream->read(&start_point.y);
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



#pragma region Avatar

void Level::Avatar::reset(Level *level)
{
    position = level->grid.cell_to_world(level->grid.start_point);

    grounded = false;
    horizontal_velocity = 0.0f;
    vertical_velocity = 0.0f;
    run_strength = 256.0f;
    friction_strength = 16.0f;
    mass = 4.0f;
    gravity = 9.81f;
    full_extent = 1.0f;
    color = random_color();
}

void Level::Avatar::step(GameInput *input, Level *level, float time_step)
{
    float horizontal_acceleration = 0.0f;
    float vertical_acceleration = 0.0f;

    bool jump = input->action(GameInput::Action::JUMP);
    bool shoot = input->action(GameInput::Action::SHOOT);

    horizontal_acceleration += input->current_horizontal_movement * run_strength;

    if(grounded)
    {
        vertical_velocity = 0.0f;

        if(jump)
        {
            vertical_velocity = 20.0f;
            grounded = false;
        }
    }
    else
    {
        //vertical_acceleration -= gravity * mass;
    }

    if(shoot)
    {
        Log::log_info("Bang!");
    }

    vertical_acceleration -= gravity * mass;
    horizontal_acceleration -= horizontal_velocity * friction_strength;

    horizontal_velocity += horizontal_acceleration * time_step;
    vertical_velocity += vertical_acceleration * time_step;
    position.x += horizontal_velocity * time_step;
    position.y += vertical_velocity * time_step;

    check_and_resolve_collisions(level);

    if(position.y < -5.0f)
    {
        level->change_mode(LOSS);
    }
}

void Level::Avatar::draw()
{
    Graphics::quad(position, v2(1.0f, 1.0f) * full_extent, 0.0f, color);
}

void Level::Avatar::check_and_resolve_collisions(Level *level)
{
    v2 avatar_bl = position - v2(1.0f, 1.0f) * full_extent * 0.5f;
    v2 avatar_tr = position + v2(1.0f, 1.0f) * full_extent * 0.5f;

    std::vector<float> rights;
    std::vector<float> lefts;
    std::vector<float> ups;
    std::vector<float> downs;
    bool won_level = false;

    v2i bl = level->grid.world_to_cell(avatar_bl) - v2i(1, 1);
    v2i tr = level->grid.world_to_cell(avatar_tr) + v2i(1, 1);
    for(v2i pos = bl; pos.y <= tr.y; pos.y++)
    {
        for(pos.x = bl.x; pos.x <= tr.x; pos.x++)
        {
            if(level->grid.at(pos) == nullptr) continue;

            if(level->grid.at(pos)->filled)
            {
                v2 cell_world_position = level->grid.cell_to_world(pos);
                v2 cell_bl = cell_world_position;
                v2 cell_tr = cell_world_position + v2(1.0f, 1.0f) * level->grid.world_scale;
                v2 dir;
                float depth;
                bool collision = aabb(cell_bl, cell_tr, avatar_bl, avatar_tr, &dir, &depth);
                if(collision)
                {
                    bool blocked = false;
                    v2 n_dir = normalize(dir);
                    v2i dir_i = v2i((int)(n_dir.x), (int)(n_dir.y));
                    v2i pos_in_question = pos + dir_i;
                    if(level->grid.at(pos_in_question) && level->grid.at(pos_in_question)->filled)
                    {
                        blocked = true;
                    }

                    if(!blocked)
                    {
                        if(dir_i.x ==  1 && dir_i.y ==  0) rights.push_back(depth);
                        if(dir_i.x == -1 && dir_i.y ==  0) lefts.push_back(depth);
                        if(dir_i.x ==  0 && dir_i.y ==  1) ups.push_back(depth);
                        if(dir_i.x ==  0 && dir_i.y == -1) downs.push_back(depth);
                    }
                }
            }

            if(level->grid.at(pos)->win_when_touched)
            {
                v2 cell_world_position = level->grid.cell_to_world(pos);
                v2 cell_bl = cell_world_position;
                v2 cell_tr = cell_world_position + v2(1.0f, 1.0f) * level->grid.world_scale;
                v2 dir;
                float depth;
                bool collision = aabb(cell_bl, cell_tr, avatar_bl, avatar_tr, &dir, &depth);
                if(collision)
                {
                    won_level = true;
                }
            }
        }
    }

    v2 resolution = v2();
    float max_right = 0.0f;
    float max_left = 0.0f;
    float max_up = 0.0f;
    float max_down = 0.0f;
    for(int i = 0; i < rights.size(); i++) { max_right = max(rights[i], max_right); }
    for(int i = 0; i < lefts.size(); i++)  { max_left  = max(lefts[i], max_left); }
    for(int i = 0; i < ups.size(); i++)    { max_up    = max(ups[i], max_up); }
    for(int i = 0; i < downs.size(); i++)  { max_down  = max(downs[i], max_down); }
    resolution += v2( 1.0f,  0.0f) * max_right;
    resolution += v2(-1.0f,  0.0f) * max_left;
    resolution += v2( 0.0f,  1.0f) * max_up;
    resolution += v2( 0.0f, -1.0f) * max_down;

    position += resolution;

    if(length_squared(resolution) == 0.0f)
    {
        grounded = false;
    }
    if(resolution.y > 0.0f && vertical_velocity < 0.0f)
    {
        grounded = true;
    }
    if(resolution.y < 0.0f && vertical_velocity > 0.0f)
    {
        vertical_velocity = 0.0f;
    }
    if(GameMath::abs(resolution.x) > 0.0f)
    {
        horizontal_velocity = 0.0f;
    }

    if(won_level)
    {
        level->change_mode(Level::Mode::WIN);
    }
}

#pragma endregion



void Level::Editor::step(Level *level, float time_step)
{
    int x;
    int y;
    Platform::Input::mouse_screen_position(&x, &y);
    v2 mouse_pos = v2(x, y);

    bool allow_placement = !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive()
        && !Platform::Input::mouse_button_down(0); // For now, don't allow placement the first frame the mouse is down
                                                   // so ImGui has time to update

    // Move camera
    if(Platform::Input::mouse_button(1))
    {
        v2 mouse_delta = mouse_pos - last_mouse_position;
        mouse_delta.y *= -1.0f;
        camera_position += -mouse_delta * 0.05f;
    }
    last_mouse_position = mouse_pos;
    Graphics::Camera::position() = camera_position;

    // Set start point
    if(placing_start_point && allow_placement)
    {
        if(Platform::Input::mouse_button_down(0))
        {
            v2 pos = Platform::Input::mouse_world_position();
            v2i grid_pos = level->grid.world_to_cell(pos);
            level->grid.start_point = grid_pos;
        }
    }
    Graphics::quad(level->grid.cell_to_world(level->grid.start_point), v2(1.0f, 1.0f), 0.0f, v4(0.0f, 1.0f, 0.0f, 1.0f));

    // Place terrain
    if(placing_terrain && allow_placement)
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

    if(placing_win_points && allow_placement)
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
        Engine::switch_game_state(GameState::MAIN_MENU);
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
                    level->load_with_file(temp_input_buffer, true);
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
                    level->load_with_file(temp_input_buffer, false);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }


        ImGui::EndPopup();
    }
}



void Level::step(GameInputList inputs, float time_step)
{
    switch(current_mode)
    {
        case PLAYING: { playing_step(inputs, time_step); break; }
        case PAUSED:  { paused_step(inputs, time_step); break; }
        case WIN:     { win_step(inputs, time_step); break; }
        case LOSS:    { loss_step(inputs, time_step); break; }
    }
}

void Level::draw(GameInput::UID local_uid)
{
    switch(current_mode)
    {
        case PLAYING: { playing_draw(local_uid); break; }
        case PAUSED:  { paused_draw(local_uid); break; }
        case WIN:     { win_draw(local_uid); break; }
        case LOSS:    { loss_draw(local_uid); break; }
    }

}

void Level::serialize(Serialization::Stream *stream)
{
    stream->write((int)avatars.size());
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        GameInput::UID uid = pair.first;
        Avatar *avatar = pair.second;

        stream->write(uid);
        stream->write(avatar->position);
        stream->write(avatar->color);
    }

    grid.serialize(stream, true);
}

void Level::deserialize(Serialization::Stream *stream)
{
    int num_avatars;
    stream->read(&num_avatars);
    std::vector<GameInput::UID> uids_seen;
    for(int i = 0; i < num_avatars; i++)
    {
        GameInput::UID uid;
        stream->read((int *)&uid);
        uids_seen.push_back(uid);

        Avatar *avatar = nullptr;
        auto it = avatars.find(uid);
        if(it == avatars.end())
        {
            avatar = add_avatar(uid);
        }
        else
        {
            avatar = it->second;
        }
        stream->read(&avatar->position);
        stream->read(&avatar->color);
    }

    // Remove "dangling" avatars
    // TODO: Fix this along with sync'ing avatars in step function (1/1/2021)
    std::vector<GameInput::UID> uids_to_remove;
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        auto it = std::find_if(uids_seen.begin(), uids_seen.end(), [&](const GameInput::UID &other_uid) { return other_uid == pair.first; });
        if(it == uids_seen.end())
        {
            uids_to_remove.push_back(pair.first);
        }
    }
    while(uids_to_remove.size() > 0)
    {
        remove_avatar(uids_to_remove.back());
        uids_to_remove.pop_back();
    }

    // TODO:
    // Here, we clear the map each time we need to serialize because we only set filled blocks,
    // without clearing ones that aren't set in the stream
    // This should be made to work as a diff fix thingy
    grid.clear();
    grid.serialize(stream, false);
}

void Level::clear()
{
    number = -1;

    grid.init();
    grid.world_scale = 1.0f;
    grid.clear();

    avatars.clear();
}

void Level::init(int level_num)
{
    clear();

    LevelsState::LevelFilesMap::iterator it = Levels::instance->level_files.find(level_num);
    if(it != Levels::instance->level_files.end())
    {
        char *path = it->second;
        load_with_file(path, true);
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

void Level::change_mode(Mode new_mode)
{
    current_mode = new_mode;
}

void Level::cleanup()
{
}

v2 Level::get_avatar_position(GameInput::UID id)
{
    auto it = avatars.find(id);
    if(it == avatars.end())
    {
        return v2();
    }
    else
    {
        return it->second->position;
    }
}

#if DEBUG
void Level::draw_debug_ui()
{
    if(ImGui::BeginTabItem("Level"))
    {
        static char load_level_buff[64] = "assets/data/levels/";
        if(ImGui::InputText("Set level 0", load_level_buff, sizeof(load_level_buff) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            // TODO: Temporary leak until the level table is formailzed
            char *file_path = new char[64];
            strcpy(file_path, load_level_buff);
            load_with_file(load_level_buff, true);
        }
        ImGui::EndTabItem();
    }
}
#endif

Level::Avatar *Level::add_avatar(GameInput::UID id)
{
    Avatar *new_avatar = new Avatar();
    new_avatar->reset(this);
    avatars[id] = new_avatar;

    return new_avatar;
}

void Level::remove_avatar(GameInput::UID id)
{
    avatars.erase(id);
}

GameInput *Level::get_input(GameInputList *inputs, GameInput::UID uid)
{
    for(int i = 0; i < inputs->size(); i++)
    {
        if((*inputs)[i].uid == uid)
        {
            return &(*inputs)[i];
        }
    }

    return nullptr;
}

void Level::playing_step(GameInputList inputs, float time_step)
{
    // Sync and match level avatars to game inputs
    // Each game input should map to one avatar to control
    // TODO: This should be done differently
    for(GameInput &input : inputs)
    {
        auto it = avatars.find(input.uid);
        if(it == avatars.end())
        {
            add_avatar(input.uid);
        }
    }
    std::vector<GameInput::UID> uids_to_remove;
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        auto it = std::find_if(inputs.begin(), inputs.end(), [&](const GameInput &input) { return input.uid == pair.first; });
        if(it == inputs.end())
        {
            uids_to_remove.push_back(pair.first);
        }
    }
    while(uids_to_remove.size() > 0)
    {
        remove_avatar(uids_to_remove.back());
        uids_to_remove.pop_back();
    }

    // Update all avatars in the level
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        GameInput *input = get_input(&inputs, pair.first);
        Avatar *avatar = pair.second;
        avatar->step(input, this, time_step);
    }

    // Check local input for menus
    // This is assuming that the platform input has been read at this point
    if(Platform::Input::key_down(Platform::Input::Key::ESC))
    {
        change_mode(PAUSED);
    }
    if(Platform::Input::key_down('R'))
    {
        init_default_level();
    }

}

void Level::paused_step(GameInputList inputs, float time_step)
{
}

void Level::win_step(GameInputList inputs, float time_step)
{

}

void Level::loss_step(GameInputList inputs, float time_step)
{
}

void Level::playing_draw(GameInput::UID local_uid)
{
    general_draw(local_uid);
}

static void begin_base_menu()
{
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    //window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoNav;
    //window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    bool *p_open = NULL;

    ImGui::SetNextWindowPos( ImVec2(0, 0) );
    ImGui::SetNextWindowSize( ImVec2(Platform::Window::screen_width(), Platform::Window::screen_height()) );
    ImGui::Begin("Menu", p_open, window_flags);
}

static void end_base_menu()
{
    ImGui::End();
}

static ImVec2 get_button_size()
{
    float button_scalar = Platform::Window::screen_width() * 0.1f;
    ImVec2 button_size = ImVec2(button_scalar, button_scalar * 0.25f);
    return button_size;
}

void Level::paused_draw(GameInput::UID local_uid)
{
    general_draw(local_uid);

    // Draw pause menu UI
    ImGui::Begin("Pause Window");
    if(ImGui::Button("Resume"))    change_mode(PLAYING);
    if(ImGui::Button("Lobby"))     Engine::switch_game_state(GameState::LOBBY);
    if(ImGui::Button("Main Menu")) Engine::switch_game_state(GameState::MAIN_MENU);
    if(ImGui::Button("Quit Game")) Engine::stop();
    ImGui::End();
}

void Level::win_draw(GameInput::UID local_uid)
{
    general_draw(local_uid);

    begin_base_menu();

    ImVec2 button_size = get_button_size();

    int next_level_num = number + 1;
    if(next_level_num < Levels::instance->level_files.size())
    {
        if(ImGui::Button("Next Level", button_size))
        {
            Levels::start_level(next_level_num);
        }
    }
    if(ImGui::Button("Main Menu", button_size)) Engine::switch_game_state(GameState::MAIN_MENU);
    if(ImGui::Button("Quit Game", button_size)) Engine::stop();

    end_base_menu();
}

void Level::loss_draw(GameInput::UID local_uid)
{
    general_draw(local_uid);

    begin_base_menu();

    ImVec2 button_size = get_button_size();
    if(ImGui::Button("Restart", button_size))
    {
        Levels::start_level(number);
    }
    if(ImGui::Button("Main Menu", button_size)) Engine::switch_game_state(GameState::MAIN_MENU);
    if(ImGui::Button("Quit Game", button_size)) Engine::stop();

    end_base_menu();
}

void Level::general_draw(GameInput::UID local_uid)
{
    v2 camera_offset = v2(16.0f, 4.0f);

    auto focus_avatar_it = avatars.find(local_uid);
    if(focus_avatar_it != avatars.end())
    {
        Avatar *focus_avatar = focus_avatar_it->second;
        Graphics::Camera::position() = focus_avatar->position + camera_offset;
    }
    Graphics::Camera::width() = 64.0f;

    // Draw the players
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        Avatar *avatar = pair.second;
        avatar->draw();
    }

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

void Level::load_with_file(const char *path, bool reading)
{
    if(reading)
    {
        Serialization::Stream *stream = Serialization::make_stream_from_file(path);
        if(stream)
        {
            deserialize(stream);
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
    else
    {
        Serialization::Stream *stream = Serialization::make_stream();
        serialize(stream);
        stream->write_to_file(path);
        Serialization::free_stream(stream);
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
    level->cleanup();
    delete level;
}

Level *Levels::active_level()
{
    return instance->active_level;
}

Level *Levels::start_level(int level_number)
{
    instance->active_level->uninit();
    delete instance->active_level;

    instance->active_level = new Level();
    instance->active_level->init(level_number);
    return instance->active_level;
}
