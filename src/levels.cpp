
#include "levels.h"
#include "platform.h"
#include "logging.h"
#include "game_math.h"
#include "algorithms.h"
#include "data_structures.h"
#include "graphics.h"
#include "serialization.h"

//#include "imgui.h"
#include <map>
#include <algorithm>



using namespace GameMath;



struct v2i
{
    union
    {
        struct { int row, col; };
        struct { int x, y; };
        int v[2];
    };

    v2i() {}
    v2i(int a, int b) : x(a), y(b) {}

    v2i operator+(v2i b) { return v2i(x + b.x, y + b.y); }
    v2i operator-(v2i b) { return v2i(x - b.x, y - b.y); }
};

struct Grid
{
    struct Cell
    {
        bool filled;
    };

    // How big is a grid cell in world space?
    float world_scale;
    int width, height;
    Cell *cells;



    void init(int w, int h)
    {
        width = w;
        height = h;
        cells = new Cell[w * h];
    }

    void clear()
    {
        for(int i = 0; i < width * height; i++) cells[i].filled = false;
    }

    Cell *at(v2i pos)
    {
        int left_bound = -( (width - 1) / 2 ) - 1;
        int right_bound = (width - 1) / 2;

        int bottom_bound = -( (width - 1) / 2 ) - 1;
        int top_bound = (width - 1) / 2;

        //assert(pos.x >= left_bound && pos.x <= right_bound);
        //assert(pos.y >= bottom_bound && pos.y <= top_bound);

        int array_row = ( (height - 1) - (pos.y + abs(bottom_bound)) );
        int array_col = pos.x + abs(left_bound);

        return &(cells[array_row * width + array_col]);
    }

    v2i top_left()
    {
        int left_bound = -( (width - 1) / 2 ) - 1;
        int top_bound = (width - 1) / 2;
        return v2i(left_bound, top_bound);
    } 

    v2i bottom_right()
    {
        int right_bound = (width - 1) / 2;
        int bottom_bound = -( (width - 1) / 2 ) - 1;
        return v2i(right_bound, bottom_bound);
    } 

    v2 cell_to_world(v2i pos)
    {
        v2 world_pos = v2((float)pos.x, (float)pos.y);
        return world_pos * world_scale;
    }

    v2i world_to_cell(v2 pos)
    {
        pos *= (1.0f / world_scale);
        v2i cell = v2i((int)pos.x, (int)pos.y);
        return cell;
    }
};

struct Avatar
{
    v2 position;
    v4 color;

    bool grounded;

    float horizontal_velocity;
    float vertical_velocity;

    float run_strength;
    float friction_strength;
    float mass;
    float gravity;

    float full_extent;
};


struct Levels::Level
{
    Grid grid;

    //std::vector<Avatar *> avatars;
    std::map<GameInput::UID, Avatar *> avatars;

    //std::vector<Enemy *> enemies;
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

static void reset_level(Levels::Level *level)
{
    level->grid.world_scale = 1.0f;

    level->grid.clear();
    for(v2i pos = {0, 0}; pos.x < level->grid.width / 2; pos.x++)
    {
        level->grid.at(pos)->filled = true;
    }

}

static void check_and_resolve_collisions(Levels::Level *level, Avatar *avatar)
{
    v2 avatar_bl = avatar->position - v2(1.0f, 1.0f) * avatar->full_extent * 0.5f;
    v2 avatar_tr = avatar->position + v2(1.0f, 1.0f) * avatar->full_extent * 0.5f;

    std::vector<float> rights;
    std::vector<float> lefts;
    std::vector<float> ups;
    std::vector<float> downs;

    v2i tl = level->grid.top_left();
    v2i br = level->grid.bottom_right();
    for(v2i pos = tl; pos.y >= br.y; pos.y--)
    {
        for(pos.x = tl.x; pos.x <= br.x; pos.x++)
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
                    if(level->grid.at(pos + dir_i)->filled)
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

    avatar->position += resolution;

    if(length_squared(resolution) == 0.0f)
    {
        avatar->grounded = false;
    }
    if(resolution.y > 0.0f && avatar->vertical_velocity < 0.0f)
    {
        avatar->grounded = true;
    }
    if(resolution.y < 0.0f && avatar->vertical_velocity > 0.0f)
    {
        avatar->vertical_velocity = 0.0f;
    }
    if(GameMath::abs(resolution.x) > 0.0f)
    {
        avatar->horizontal_velocity = 0.0f;
    }
}

static void init_avatar(Avatar *avatar)
{
    avatar->position = v2(2.0f, 4.0f);
    avatar->grounded = false;

    avatar->horizontal_velocity = 0.0f;
    avatar->vertical_velocity = 0.0f;

    avatar->run_strength = 256.0f;
    avatar->friction_strength = 16.0f;
    avatar->mass = 4.0f;
    avatar->gravity = 9.81f;

    avatar->full_extent = 1.0f;
    avatar->color = random_color();
}

static void step_avatar(GameInput *input, Avatar *avatar, Levels::Level *level, float dt)
{
    float horizontal_acceleration = 0.0f;
    float vertical_acceleration = 0.0f;

    bool moving_right = input->action(GameInput::Action::MOVE_RIGHT);
    bool moving_left = input->action(GameInput::Action::MOVE_LEFT);
    bool jump = input->action(GameInput::Action::JUMP);

    if(moving_right)
    {
        horizontal_acceleration += avatar->run_strength;
    }
    if(moving_left)
    {
        horizontal_acceleration -= avatar->run_strength;
    }

    if(avatar->grounded)
    {
        avatar->vertical_velocity = 0.0f;

        if(jump)
        {
            avatar->vertical_velocity = 20.0f;
            avatar->grounded = false;
        }
    }
    else
    {
        //vertical_acceleration -= avatar->gravity * avatar->mass;
    }
    vertical_acceleration -= avatar->gravity * avatar->mass;

    horizontal_acceleration -= avatar->horizontal_velocity * avatar->friction_strength;

    avatar->horizontal_velocity += horizontal_acceleration * dt;
    avatar->vertical_velocity += vertical_acceleration * dt;

    avatar->position.x += avatar->horizontal_velocity * dt;
    avatar->position.y += avatar->vertical_velocity * dt;



    check_and_resolve_collisions(level, avatar);
}

static void draw_avatar(Avatar *avatar)
{
    Graphics::draw_quad(avatar->position, v2(1.0f, 1.0f) * avatar->full_extent, 0.0f, avatar->color);
}



Levels::Level *Levels::create_level()
{
    Level* level = new Level();

    level->grid.init(256, 256);

    reset_level(level);

    return level;
}

static Avatar *add_avatar(Levels::Level *level, GameInput::UID id)
{
    Avatar *new_avatar = new Avatar();
    init_avatar(new_avatar);
    level->avatars[id] = new_avatar;

    return new_avatar;
}

static GameInput *get_input(GameInputList *inputs, GameInput::UID uid)
{
    for(int i = 0; i < inputs->size(); i++)
    {
        if((*inputs)[i].uid == uid)
        {
            return &(*inputs)[i];
        }
    }
}

void remove_avatar(Levels::Level *level, GameInput::UID id)
{
    //std::vector<Avatar *>::iterator it = std::find_if(level->avatars.begin(), level->avatars.end(),
    //    [&](const Avatar *other) { return id == other->player_id; }
    //);
    //level->avatars.erase(it);
}

void Levels::step_level(GameInputList inputs, Level *level, float dt)
{
    // BRAINDEAD PASTE
    // I don't know how to nicely match game inputs with avatars
    for(GameInput &input : inputs)
    {
        auto it = level->avatars.find(input.uid);
        if(it == level->avatars.end())
        {
            add_avatar(level, input.uid);
        }
    }
    std::vector<GameInput::UID> uids_to_remove;
    for(const std::pair<GameInput::UID, Avatar *> &pair : level->avatars)
    {
        auto it = std::find_if(inputs.begin(), inputs.end(), [&](const GameInput &input) { return input.uid == pair.first; });
        if(it == inputs.end())
        {
            uids_to_remove.push_back(pair.first);
        }
    }
    while(uids_to_remove.size() > 0)
    {
        level->avatars.erase(uids_to_remove.back());
        uids_to_remove.pop_back();
    }

    for(const std::pair<GameInput::UID, Avatar *> &pair : level->avatars)
    {
        GameInput *input = get_input(&inputs, pair.first);
        Avatar *avatar = pair.second;
        step_avatar(input, avatar, level, dt);
    }
}

void Levels::draw_level(Level *level)
{
    v2 camera_offset = v2(16.0f, 4.0f);
    Graphics::set_camera_width(64.0f);
    Graphics::set_camera_position(v2() + camera_offset);

    // Draw the players
    for(const std::pair<GameInput::UID, Avatar *> &pair : level->avatars)
    {
        Avatar *avatar = pair.second;
        draw_avatar(avatar);
    }

    // Draw grid terrain
    v2i tl = level->grid.top_left();
    v2i br = level->grid.bottom_right();
    for(v2i pos = tl; pos.y >= br.y; pos.y--)
    {
        for(pos.x = tl.x; pos.x <= br.x; pos.x++)
        {
            if(level->grid.at(pos)->filled)
            {
                float inten = (float)pos.col / level->grid.width;
                v4 color = v4(1.0f - inten, 0.0f, inten, 1.0f);
                if(pos.col == level->grid.width / 2) color = v4(1.0f, 1.0f, 1.0f, 1.0f);

                v2 world_pos = level->grid.cell_to_world(pos) + v2(1.0f, 1.0f) * level->grid.world_scale / 2.0f;
                float scale = level->grid.world_scale * 0.95f;
                Graphics::draw_quad(world_pos, v2(scale, scale), 0.0f, color);
            }
        }
    }
}

void Levels::serialize_level(Serialization::Stream *stream, Level *level)
{
    stream->write((int)level->avatars.size());
    for(const std::pair<GameInput::UID, Avatar *> &pair : level->avatars)
    {
        GameInput::UID uid = pair.first;
        Avatar *avatar = pair.second;

        stream->write(uid);
        stream->write(avatar->position);
        stream->write(avatar->color);
    }
}

void Levels::deserialize_level(Serialization::Stream *stream, Level *level)
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
        auto it = level->avatars.find(uid);
        if(it == level->avatars.end())
        {
            avatar = add_avatar(level, uid);
        }
        else
        {
            avatar = it->second;
        }
        stream->read(&avatar->position);
        stream->read(&avatar->color);
    }



    // Remove "dangling" avatars
    // BRAINDEAD FIX TOO
    std::vector<GameInput::UID> uids_to_remove;
    for(const std::pair<GameInput::UID, Avatar *> &pair : level->avatars)
    {
        auto it = std::find_if(uids_seen.begin(), uids_seen.end(), [&](const GameInput::UID &other_uid) { return other_uid == pair.first; });
        if(it == uids_seen.end())
        {
            uids_to_remove.push_back(pair.first);
        }
    }
    while(uids_to_remove.size() > 0)
    {
        level->avatars.erase(uids_to_remove.back());
        uids_to_remove.pop_back();
    }
}


