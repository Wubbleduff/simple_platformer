
#include "level.h"
#include "memory.h"
#include "my_math.h"
#include "my_algorithms.h"
#include "renderer.h"
#include "engine.h"
#include "input.h"

#include "imgui.h"

#include <vector>

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
        cells = (Cell *)calloc(sizeof(Cell), w * h);
    }

    void clear()
    {
        for(int i = 0; i < width * height; i++) cells[i].filled = false;
    }

    Cell *at(v2i pos)
    {
        return &(cells[pos.y * width + pos.x]);
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

struct Player
{
    v2 position;
    bool grounded;

    float horizontal_velocity;
    float vertical_velocity;

    float run_strength;
    float friction_strength;
    float mass;
    float gravity;

    float full_extent;

    v4 color;
};


struct Level
{
    Grid grid;

    Player player;
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

    float max_depth = max_float(left_diff, max_float(right_diff, max_float(top_diff, bottom_diff)));

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

static void reset_level(Level *level)
{
    level->grid.world_scale = 1.0f;

    level->grid.clear();
    for(v2i pos = {0, 0}; pos.x < level->grid.width; pos.x++)
    {
        level->grid.at(pos)->filled = true;
    }

    level->player.position = v2(0.0f, 4.0f);
    level->player.grounded = false;

    level->player.horizontal_velocity = 0.0f;
    level->player.vertical_velocity = 0.0f;

    level->player.run_strength = 256.0f;
    level->player.friction_strength = 16.0f;
    level->player.mass = 4.0f;
    level->player.gravity = 9.81f;

    level->player.full_extent = 1.0f;
    level->player.color = v4(1.0f, 0.0f, 0.5f, 1.0f);
}



Level *create_level()
{
    Level *level = (Level *)malloc(sizeof(Level));

    level->grid.init(256, 256);

    reset_level(level);

    return level;
}

void level_step(Level *level, float dt)
{

    InputState *input_state = get_engine_input_state();

    if(key_toggled_down(input_state, 'R'))
    {
        reset_level(level);
    }

    if(mouse_state(input_state, 0))
    {
        v2i cell = level->grid.world_to_cell(mouse_world_position(input_state));
        level->grid.at(cell)->filled = true;
    }

    Player *player = &(level->player);

    float horizontal_acceleration = 0.0f;
    float vertical_acceleration = 0.0f;

    if(key_state(input_state, 'D'))
    {
        horizontal_acceleration += player->run_strength;
    }
    if(key_state(input_state, 'A'))
    {
        horizontal_acceleration -= player->run_strength;
    }
    


    if(player->grounded)
    {
        player->vertical_velocity = 0.0f;

        if(key_toggled_down(input_state, ' '))
        {
            player->vertical_velocity = 20.0f;
            player->grounded = false;
        }
    }
    else
    {
        //vertical_acceleration -= player->gravity * player->mass;
    }
    vertical_acceleration -= player->gravity * player->mass;

    horizontal_acceleration -= player->horizontal_velocity * player->friction_strength;

    player->horizontal_velocity += horizontal_acceleration * dt;
    player->vertical_velocity += vertical_acceleration * dt;

    player->position.x += player->horizontal_velocity * dt;
    player->position.y += player->vertical_velocity * dt;


    RendererState *renderer_state = get_engine_renderer_state();

    {
        v2 player_bl = player->position - v2(1.0f, 1.0f) * player->full_extent * 0.5f;
        v2 player_tr = player->position + v2(1.0f, 1.0f) * player->full_extent * 0.5f;

        std::vector<float> rights;
        std::vector<float> lefts;
        std::vector<float> ups;
        std::vector<float> downs;

        for(v2i cell = {0, 0}; cell.row < level->grid.height; cell.row++)
        {
            for(cell.col = 0; cell.col < level->grid.width; cell.col++)
            {
                if(level->grid.at(cell)->filled)
                {
                    v2 cell_world_position = level->grid.cell_to_world(cell);

                    v2 cell_bl = cell_world_position;
                    v2 cell_tr = cell_world_position + v2(1.0f, 1.0f) * level->grid.world_scale;

                    v2 dir;
                    float depth;
                    bool collision = aabb(cell_bl, cell_tr, player_bl, player_tr, &dir, &depth);
                    if(collision)
                    {
                        bool blocked = false;
                        v2 n_dir = normalize(dir);
                        v2i dir_i = v2i((int)(n_dir.x), (int)(n_dir.y));
                        if(level->grid.at(cell + dir_i)->filled)
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
        for(float f : rights) { max_right = max_float(f, max_right); }
        for(float f : lefts)  { max_left  = max_float(f, max_left); }
        for(float f : ups)    { max_up    = max_float(f, max_up); }
        for(float f : downs)  { max_down  = max_float(f, max_down); }
        resolution += v2( 1.0f,  0.0f) * max_right;
        resolution += v2(-1.0f,  0.0f) * max_left;
        resolution += v2( 0.0f,  1.0f) * max_up;
        resolution += v2( 0.0f, -1.0f) * max_down;

        player->position += resolution;

        if(length_squared(resolution) == 0.0f)
        {
            player->grounded = false;
        }
        if(resolution.y > 0.0f && player->vertical_velocity < 0.0f)
        {
            player->grounded = true;
        }
        if(resolution.y < 0.0f && player->vertical_velocity > 0.0f)
        {
            player->vertical_velocity = 0.0f;
        }
        if(absf(resolution.x) > 0.0f)
        {
            player->horizontal_velocity = 0.0f;
        }
    }




    v2 camera_offset = v2(16.0f, 4.0f);
    set_camera_width(renderer_state, 64.0f);
    set_camera_position(renderer_state, player->position + camera_offset);

    draw_quad(renderer_state, v2(), v2(0.25f, 0.25f), PI / 4.0f, v4(0.0f, 1.0f, 0.0f, 1.0f));

#if 1
    for(v2i pos = {0, 0}; pos.row < level->grid.height; pos.row++)
    {
        for(pos.col = 0; pos.col < level->grid.width; pos.col++)
        {
            if(level->grid.at(pos)->filled)
            {
                float inten = (float)pos.col / level->grid.width;
                v4 color = v4(1.0f - inten, 0.0f, inten, 1.0f);
                if(pos.col == level->grid.width / 2) color = v4(1.0f, 1.0f, 1.0f, 1.0f);

                v2 world_pos = level->grid.cell_to_world(pos) + v2(1.0f, 1.0f) * level->grid.world_scale / 2.0f;
                float scale = level->grid.world_scale * 0.95f;
                draw_quad(renderer_state, world_pos, v2(scale, scale), 0.0f, color);
            }
        }
    }

    draw_quad(renderer_state, player->position, v2(player->full_extent, player->full_extent), 0.0f, player->color);

#endif


#if 0
    v2 a_bl = v2(-1.0f, -1.0f);
    v2 a_tr = v2( 1.0f,  1.0f);

    v2 mouse_pos = mouse_world_position(input_state);

    v2 b_bl = v2(-1.0f, -1.0f) + mouse_pos;
    v2 b_tr = v2( 1.0f,  1.0f) + mouse_pos;

    v2 dir;
    float depth;
    bool hit = aabb(a_bl, a_tr, b_bl, b_tr, &dir, &depth);

    v4 color = v4(1.0f, 0.0f, 0.0f, 1.0f);
    if(hit)
    {
        color = v4(0.0f, 1.0f, 0.0f, 1.0f);
    }

    v2 a_center = (a_bl + a_tr) / 2.0f;
    v2 b_center = (b_bl + b_tr) / 2.0f;
    draw_quad(renderer_state, a_center, a_tr - a_bl, 0.0f, color);
    draw_quad(renderer_state, b_center, b_tr - b_bl, 0.0f, color);

    if(hit)
    {
        draw_line(renderer_state, a_tr, a_tr + (dir * depth), v4(1.0f, 1.0f, 0.0f, 1.0f));
        draw_quad(renderer_state, b_center + dir * depth, b_tr - b_bl, 0.0f, v4(0.0f, 1.0f, 0.0f, 0.5f));
    }
#endif
}

