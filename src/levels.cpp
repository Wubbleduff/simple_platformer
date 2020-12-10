
#include "levels.h"
#include "platform.h"
#include "logging.h"
#include "game_math.h"
#include "algorithms.h"
#include "data_structures.h"
#include "graphics.h"
#include "serialization.h"

//#include "imgui.h"



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
        cells = (Cell *)Platform::Memory::allocate(sizeof(Cell) * w * h);
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
    Game::PlayerID player_id;

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

    DynamicArray<Avatar *> avatars;
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

    DynamicArray<float> rights;
    DynamicArray<float> lefts;
    DynamicArray<float> ups;
    DynamicArray<float> downs;

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
    for(int i = 0; i < rights.size; i++) { max_right = max(rights[i], max_right); }
    for(int i = 0; i < lefts.size; i++)  { max_left  = max(lefts[i], max_left); }
    for(int i = 0; i < ups.size; i++)    { max_up    = max(ups[i], max_up); }
    for(int i = 0; i < downs.size; i++)  { max_down  = max(downs[i], max_down); }
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
    if(abs(resolution.x) > 0.0f)
    {
        avatar->horizontal_velocity = 0.0f;
    }
}

static void init_avatar(Avatar *avatar, Game::PlayerID id)
{
    avatar->player_id = id;

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

static void step_avatar(Avatar *avatar, Levels::Level *level, float dt)
{
    float horizontal_acceleration = 0.0f;
    float vertical_acceleration = 0.0f;

    bool moving_right = Game::Players::action(avatar->player_id, Game::Players::Action::MOVE_RIGHT);
    bool moving_left = Game::Players::action(avatar->player_id, Game::Players::Action::MOVE_LEFT);
    bool jump = Game::Players::action(avatar->player_id, Game::Players::Action::JUMP);

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
    Level* level = (Level *)Platform::Memory::allocate(sizeof(Level));

    level->avatars.init();

    level->grid.init(256, 256);

    reset_level(level);

    return level;
}

void Levels::add_avatar(Level *level, Game::PlayerID id)
{
    Avatar *new_avatar = (Avatar *)Platform::Memory::allocate(sizeof(Avatar));
    init_avatar(new_avatar, id);
    level->avatars.push_back(new_avatar);
}

void Levels::remove_avatar(Level *level, Game::PlayerID id)
{
    /*
    Avatar *to_remove = nullptr;
    int to_remove_index = -1;
    for(int i = 0; i < level->avatars.size; i++)
    {
        if(level->avatars[i]->player_id == id)
        {
            to_remove = level->avatars[i];
            to_remove_index = i;
            break;
        }
    }
    if(to_remove == nullptr) return;

    Platform::Memory::free(to_remove);

    level->avatars.remove_unordered(to_remove_index);
    */
}

void Levels::step_level(Level *level, float dt)
{
    for(int i = 0; i < level->avatars.size; i++)
    {
        Avatar *avatar = level->avatars[i];
        step_avatar(avatar, level, dt);
    }
}

void Levels::draw_level(Level *level)
{
    v2 camera_offset = v2(16.0f, 4.0f);
    Graphics::set_camera_width(64.0f);
    Graphics::set_camera_position(v2() + camera_offset);

    // Draw the players
    for(int i = 0; i < level->avatars.size; i++)
    {
        Avatar *avatar = level->avatars[i];
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
    // Write the header
    /*
    Serialization::write_stream(stream, level->grid.width);
    Serialization::write_stream(stream, level->grid.height);
    

    // Write the body
    v2i tl = level->grid.top_left();
    v2i br = level->grid.bottom_right();
    for(v2i pos = tl; pos.y >= br.y; pos.y--)
    {
        for(pos.x = tl.x; pos.x <= br.x; pos.x++)
        {
            if(level->grid.at(pos)->filled)
            {
                Serialization::write_stream(stream, '1');
            }
            else
            {
                Serialization::write_stream(stream, '0');
            }
        }
    }
    */

    stream->write(level->avatars.size);
    for(int i = 0; i < level->avatars.size; i++)
    {
        stream->write(level->avatars[i]->position);
        stream->write(level->avatars[i]->color);
    }
    
}

void Levels::deserialize_level(Serialization::Stream *stream, Level *level)
{
    // Read the header
    /*
    Serialization::read_stream(stream, &(level->grid.width));
    Serialization::read_stream(stream, &(level->grid.height));
    

    // Read the body
    v2i tl = level->grid.top_left();
    v2i br = level->grid.bottom_right();
    for(v2i pos = tl; pos.y >= br.y; pos.y--)
    {
        for(pos.x = tl.x; pos.x <= br.x; pos.x++)
        {
            char c;
            Serialization::read_stream(stream, &c);
            level->grid.at(pos)->filled = (c == '1');
        }
    }
    */

    int num_avatars;
    stream->read(&num_avatars);
    while(level->avatars.size < num_avatars)
    {
        // TODO: Match server players to avatars
        Levels::add_avatar(level, { -1 });
    }
    for(int i = 0; i < num_avatars; i++)
    {
        stream->read(&(level->avatars[i]->position));
        stream->read(&(level->avatars[i]->color));
    }
    
}


