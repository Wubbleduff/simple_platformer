
#include "level.h"
#include "memory.h"
#include "my_math.h"
#include "my_algorithms.h"
#include "renderer.h"
#include "engine.h"
#include "input.h"

#include "imgui.h"

struct v2i
{
    union
    {
        struct { int row, col; };
        struct { int x, y; };
        int v[2];
    };
};

struct Grid
{
    struct Cell
    {
        bool filled;
    };

    int width, height;
    Cell *cells;



    void init(int w, int h)
    {
        width = w;
        height = h;
        cells = (Cell *)calloc(sizeof(Cell), w * h);
    }

    Cell *at(v2i pos)
    {
        return &(cells[pos.row * width + pos.col]);
    }
};

struct Player
{
    v2 position;
    v2 velocity;

    float acceleration_strength;
    float friction_strength;

    float full_extent;

    v4 color;
};


struct Level
{
    Grid grid;

    Player player;
};

Level *create_level()
{
    Level *level = (Level *)malloc(sizeof(Level));

    level->grid.init(256, 256);

    
    for(v2i pos = {level->grid.height / 2, 0}; pos.col < level->grid.width; pos.col++)
    {
        level->grid.at(pos)->filled = true;
    }

    level->player.position = v2(0.0f, 1.5f);
    level->player.velocity = v2();
    level->player.acceleration_strength = 256.0f;
    level->player.friction_strength = 16.0f;
    level->player.full_extent = 1.0f;
    level->player.color = v4(1.0f, 0.0f, 0.5f, 1.0f);

    return level;
}

void level_step(Level *level, float dt)
{
    InputState *input_state = get_engine_input_state();
    Player *player = &(level->player);

    v2 acceleration = v2();
    if(key_state(input_state, 'D'))
    {
        acceleration.x += player->acceleration_strength * dt;
    }
    if(key_state(input_state, 'A'))
    {
        acceleration.x -= player->acceleration_strength * dt;
    }

    acceleration -= player->velocity * player->friction_strength * dt;

    player->velocity += acceleration;
    player->position += player->velocity * dt;



    RendererState *renderer_state = get_engine_renderer_state();
    set_camera_width(renderer_state, 64.0f);
    set_camera_position(renderer_state, v2(24.0f, 14.0f));
    for(v2i pos = {0, 0}; pos.row < level->grid.height; pos.row++)
    {
        for(pos.col = 0; pos.col < level->grid.width; pos.col++)
        {
            if(level->grid.at(pos)->filled)
            {
                float level_width = 256.0f;
                v2 world_pos = remap( v2(pos.col, pos.row),
                        v2( 0.0f, 0.0f), v2(255.0f, 255.0f),
                        v2(-level_width/2.0f, level_width/2.0f), v2(level_width/2.0f, -level_width/2.0f) );
                float scale = (level_width / level->grid.width) * 0.9f;

                float inten = (float)pos.col / level->grid.width;
                v4 color = v4(1.0f - inten, 0.0f, inten, 1.0f);
                if(pos.col == level->grid.width / 2) color = v4(1.0f, 1.0f, 1.0f, 1.0f);

                draw_quad(renderer_state, world_pos, v2(scale, scale), 0.0f, color);
            }
        }
    }

    draw_quad(renderer_state, player->position, v2(player->full_extent, player->full_extent), 0.0f, player->color);

    draw_quad(renderer_state, v2(), v2(0.25f, 0.25f), PI / 4.0f, v4(0.0f, 1.0f, 0.0f, 1.0f));

}

