
#pragma once

#include "game.h"
#include "game_math.h"
#include "serialization.h"

#include <map>

struct Level
{
public:
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

        void init(int w, int h);
        void clear();
        Cell *at(v2i pos);
        v2i top_left();
        v2i bottom_right();
        GameMath::v2 cell_to_world(v2i pos);
        v2i world_to_cell(GameMath::v2 pos);
    };

    struct Avatar
    {
        GameMath::v2 position;
        GameMath::v4 color;
        bool grounded;
        float horizontal_velocity;
        float vertical_velocity;
        float run_strength;
        float friction_strength;
        float mass;
        float gravity;
        float full_extent;

        void reset();
        void step(GameInput *input, Level *level, float time_step);
        void draw();
        void check_and_resolve_collisions(Level *level);
    };

    enum Mode
    {
        PLAYING,
        PAUSED,
        WIN,
        LOSS
    };

    Grid grid;
    std::map<GameInput::UID, Avatar *> avatars;
    Mode current_mode;

    void step(GameInputList inputs, float time_step);
    void draw();
    void serialize(Serialization::Stream *stream);
    void deserialize(Serialization::Stream *stream);
    void reset();
    void change_mode(Mode new_mode);
    void cleanup();


private:
    Avatar *Level::add_avatar(GameInput::UID id);
    void remove_avatar(GameInput::UID id);
    GameInput *get_input(GameInputList *inputs, GameInput::UID uid);

    void playing_step(GameInputList inputs, float time_step);
    void paused_step(GameInputList inputs, float time_step);
    void win_step(GameInputList inputs, float time_step);
    void loss_step(GameInputList inputs, float time_step);

    void playing_draw();
    void paused_draw();
    void win_draw();
    void loss_draw();
    void general_draw();
};

Level *create_level();
void destroy_level(Level *level);
