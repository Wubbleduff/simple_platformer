
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



// TODO: Global until I find a better place for this
typedef std::map<int, char *> LevelFilessMap;
static LevelFilessMap level_files =
{
    {0, "data/levels/level_0"}
};



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



void Level::Grid::Cell::reset()
{
    filled = false;
    win_when_touched = false;
}

void Level::Grid::Cell::serialize(Serialization::Stream *stream, bool writing)
{
    if(writing)
    {
        stream->write(filled ? 1 : 0);
        stream->write(win_when_touched ? 1 : 0);
    }
    else
    {
        int val;
        stream->read(&val);
        filled = (val == 0 ? false : true);

        stream->read(&val);
        win_when_touched = (val == 0 ? false : true);
    }
}

void Level::Grid::init(int w, int h)
{
    delete[] cells;
    width = w;
    height = h;
    cells = new Cell[w * h];
}

void Level::Grid::clear()
{
    for(int i = 0; i < width * height; i++) cells[i].reset();
}

Level::Grid::Cell *Level::Grid::at(v2i pos)
{
    bool valid = valid_position(pos);
    assert(valid);

    v2i memory_coord = grid_coordinate_to_memory_coordinate(pos);
    return &(cells[memory_coord.y * width + memory_coord.x]);
}

bool Level::Grid::valid_position(v2i pos)
{
    v2i memory_coord = grid_coordinate_to_memory_coordinate(pos);
    if(memory_coord.x < 0 || memory_coord.x >= width)  return false;
    if(memory_coord.y < 0 || memory_coord.y >= height) return false;
    return true;
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
    return cell;
}

Level::v2i Level::Grid::bottom_left()
{
    return {-width / 2, -height / 2};
}

Level::v2i Level::Grid::top_right()
{
    return {width / 2 - 1, height / 2 - 1};
}

Level::v2i Level::Grid::grid_coordinate_to_memory_coordinate(v2i grid_coord)
{
    /*             X  Y
     * get 0, 0 -> 4, 3
          0 1 2 3 4 5 6 7
        0
        1
        2
        3       x x <-- will pick this one if width and height are even (top-left one)
        4       x x
        5
        6
        7
        
    */
    v2i result = grid_coord;
    result.x += width / 2;  // Move selection to middle of memory grid
    result.y = (height - 1) - result.y; // Invert the y selection
    result.y -= height / 2; // Move selection to middle of memory grid
    return result;
}



void Level::Avatar::reset(Level *level)
{
    position = v2(5, 20);
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

    bool moving_right = input->action(GameInput::Action::MOVE_RIGHT);
    bool moving_left = input->action(GameInput::Action::MOVE_LEFT);
    bool jump = input->action(GameInput::Action::JUMP);

    if(moving_right)
    {
        horizontal_acceleration += run_strength;
    }
    if(moving_left)
    {
        horizontal_acceleration -= run_strength;
    }

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
    Graphics::draw_quad(position, v2(1.0f, 1.0f) * full_extent, 0.0f, color);
}

void Level::Avatar::check_and_resolve_collisions(Level *level)
{
    v2 avatar_bl = position - v2(1.0f, 1.0f) * full_extent * 0.5f;
    v2 avatar_tr = position + v2(1.0f, 1.0f) * full_extent * 0.5f;

    std::vector<float> rights;
    std::vector<float> lefts;
    std::vector<float> ups;
    std::vector<float> downs;
    bool won_game = false;

    v2i bl = level->grid.bottom_left();
    v2i tr = level->grid.top_right();
    for(v2i pos = bl; pos.y <= tr.y; pos.y++)
    {
        for(pos.x = bl.x; pos.x <= tr.x; pos.x++)
        {
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
                    if(level->grid.valid_position(pos_in_question) && level->grid.at(pos_in_question)->filled)
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
                    won_game = true;
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

    if(won_game)
    {
        level->change_mode(Level::Mode::WIN);
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

void Level::edit_step(float time_step)
{
    if(Platform::Input::mouse_button(1))
    {
        v2 pos = Platform::Input::mouse_world_position();
        v2i grid_pos = grid.world_to_cell(pos);
        if(grid.valid_position(grid_pos))
        {
            grid.at(grid_pos)->filled = true;
        }
    }

    if(Platform::Input::key_down('W'))
    {
        v2 pos = Platform::Input::mouse_world_position();
        v2i grid_pos = grid.world_to_cell(pos);
        if(grid.valid_position(grid_pos))
        {
            grid.at(grid_pos)->win_when_touched = true;
        }
    }

    if(Platform::Input::key_down('M'))
    {
        Game::exit_to_main_menu();
    }
}

void Level::draw()
{
    switch(current_mode)
    {
        case PLAYING: { playing_draw(); break; }
        case PAUSED:  { paused_draw(); break; }
        case WIN:     { win_draw(); break; }
        case LOSS:    { loss_draw(); break; }
    }
}

void Level::edit_draw()
{
    general_draw();

    ImGui::Begin("Editor");
    static char load_buff[64] = "data/levels/";
    if(ImGui::InputText("Load level", load_buff, sizeof(load_buff) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        load_with_file(load_buff, true);
    }

    static char save_buff[64] = "data/levels/";
    if(ImGui::InputText("Save level", save_buff, sizeof(save_buff) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        load_with_file(save_buff, false);
    }

    static char level_0_buff[64] = "data/levels/";
    if(ImGui::InputText("Set level 0", level_0_buff, sizeof(level_0_buff) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        // TODO: Temporary leak until the level table is formailzed
        char *file_path = new char[64];
        strcpy(file_path, level_0_buff);
        level_files[0] = file_path;
    }
    ImGui::End();
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

    stream->write(grid.width);
    stream->write(grid.height);
    stream->write(grid.world_scale);
    for(int i = 0; i < grid.width * grid.height; i++)
    {
        grid.cells[i].serialize(stream, true);
    }
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

    int w, h;
    stream->read(&w);
    stream->read(&h);
    grid.init(w, h);
    stream->read(&grid.world_scale);
    for(int i = 0; i < grid.width * grid.height; i++)
    {
        grid.cells[i].serialize(stream, false);
    }
}

void Level::reset(int level_num)
{
    LevelFilessMap::iterator it = level_files.find(level_num);
    if(it != level_files.end())
    {
        char *path = it->second;
        load_with_file(path, true);
    }
    else
    {
        Log::log_error("Could not find file associated with level %i", level_num);
        reset_to_default_level();
    }
}

void Level::reset_to_default_level()
{
    grid.init(256, 256);
    grid.world_scale = 1.0f;
    grid.clear();
    for(v2i pos = {0, 0}; pos.x < grid.width / 2; pos.x++)
    {
        grid.at(pos)->filled = true;
    }

    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        Avatar *avatar = pair.second;
        avatar->reset(this);
    }
}

void Level::change_mode(Mode new_mode)
{
    current_mode = new_mode;
}

void Level::cleanup()
{
    delete [] grid.cells;
}

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
    if(Platform::Input::key_down('M'))
    {
        change_mode(PAUSED);
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

void Level::playing_draw()
{
    general_draw();
}

void Level::paused_draw()
{
    general_draw();

    // Draw pause menu UI
    ImGui::Begin("Pause Window");
    if(ImGui::Button("Resume"))    change_mode(PLAYING);
    if(ImGui::Button("Main Menu")) Game::exit_to_main_menu();
    if(ImGui::Button("Quit Game")) Game::stop();
    ImGui::End();
}

void Level::win_draw()
{
    ImGui::Begin("You win!!!");
    if(ImGui::Button("Main Menu")) Game::exit_to_main_menu();
    if(ImGui::Button("Quit Game")) Game::stop();
    ImGui::End();
}

void Level::loss_draw()
{
    general_draw();

    ImGui::Begin("You lose!");
    if(ImGui::Button("Restart"))
    {
        reset(0);
        change_mode(PLAYING);
    }
    if(ImGui::Button("Main Menu")) Game::exit_to_main_menu();
    if(ImGui::Button("Quit Game")) Game::stop();
    ImGui::End();
}

void Level::general_draw()
{
    v2 camera_offset = v2(16.0f, 4.0f);

    GameInput::UID my_uid = Game::get_my_uid();
    auto focus_avatar_it = avatars.find(my_uid);
    if(focus_avatar_it != avatars.end())
    {
        Avatar *focus_avatar = focus_avatar_it->second;
        Graphics::set_camera_position(focus_avatar->position + camera_offset);
    }
    Graphics::set_camera_width(64.0f);

    // Draw the players
    for(const std::pair<GameInput::UID, Avatar *> &pair : avatars)
    {
        Avatar *avatar = pair.second;
        avatar->draw();
    }

    // Draw grid terrain
    v2i bl = grid.bottom_left();
    v2i tr = grid.top_right();
    for(v2i pos = bl; pos.y <= tr.y; pos.y++)
    {
        for(pos.x = bl.x; pos.x <= tr.x; pos.x++)
        {
            if(grid.at(pos)->filled)
            {
                float inten = (float)pos.x / grid.width;
                v4 color = v4(1.0f - inten, 0.0f, inten, 1.0f);
                if(pos.x == grid.width / 2) color = v4(1.0f, 1.0f, 1.0f, 1.0f);

                v2 world_pos = grid.cell_to_world(pos) + v2(1.0f, 1.0f) * grid.world_scale / 2.0f;
                float scale = grid.world_scale * 0.95f;
                Graphics::draw_quad(world_pos, v2(scale, scale), 0.0f, color);
            }

            if(grid.at(pos)->win_when_touched)
            {
                v4 color = v4(0.0f, 1.0f, 0.0f, 1.0f);
                v2 world_pos = grid.cell_to_world(pos) + v2(1.0f, 1.0f) * grid.world_scale / 2.0f;
                float scale = grid.world_scale * 0.95f;
                Graphics::draw_quad(world_pos, v2(scale, scale), 0.0f, color);
            }
        }
    }

    ImGui::Begin("Debug");
    static char load_level_buff[64] = "data/levels/";
    if(ImGui::InputText("Set level 0", load_level_buff, sizeof(load_level_buff) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        // TODO: Temporary leak until the level table is formailzed
        char *file_path = new char[64];
        strcpy(file_path, load_level_buff);
        load_with_file(load_level_buff, true);
    }
    ImGui::End();
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
        }
        else
        {
            reset_to_default_level();
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



Level *create_level(int level_num)
{
    Level *new_level = new Level();
    new_level->reset(level_num);
    return new_level;
}

void destroy_level(Level *level)
{
    level->cleanup();
    delete level;
}
