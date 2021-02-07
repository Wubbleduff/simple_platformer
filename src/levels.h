
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
            struct { int x, y; };
            int v[2];
        };

        v2i() {}
        v2i(int a, int b) : x(a), y(b) {}

        v2i operator+(v2i b) { return v2i(x + b.x, y + b.y); }
        v2i operator-(v2i b) { return v2i(x - b.x, y - b.y); }
        bool operator==(v2i b) { return (this->x == b.x) && (this->y == b.y); }
    };

    struct Grid
    {
        struct Cell
        {
            bool filled;
            bool win_when_touched;

            void reset();
        };

        struct v2iComp
        {
            bool operator()(const v2i &a, const v2i &b) const
            {
                if(a.x == b.x) return a.y < b.y;
                else return a.x < b.x;
            }
        };

        // How big is a grid cell in world space?
        float world_scale;
        std::map<v2i, Cell *, v2iComp> cells_map;

        void init();
        void clear();
        Cell *at(Level::v2i pos);
        void set_filled(Level::v2i pos, bool fill);
        GameMath::v2 cell_to_world(v2i pos);
        v2i world_to_cell(GameMath::v2 pos);
        void serialize(Serialization::Stream *stream, bool writing = true);
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

        void reset(Level *level);
        void step(GameInput *input, Level *level, float time_step);
        void draw();
        void check_and_resolve_collisions(Level *level);
    };

    struct EditState
    {
        GameMath::v2 camera_position = GameMath::v2();

        GameMath::v2 last_mouse_position = GameMath::v2();
    };

    enum Mode
    {
        PLAYING,
        PAUSED,
        WIN,
        LOSS
    };

    int number;
    Grid grid;
    std::map<GameInput::UID, Avatar *> avatars;
    Mode current_mode;
    EditState edit_state;

    void step(GameInputList inputs, float time_step);
    void edit_step(float time_step);
    void draw();
    void edit_draw();
    void serialize(Serialization::Stream *stream);
    void deserialize(Serialization::Stream *stream);
    void reset(int level_num);
    void reset_to_default_level();
    void change_mode(Mode new_mode);
    void cleanup();

    GameMath::v2 get_avatar_position(GameInput::UID id);

#if DEBUG
    void draw_debug_ui();
#endif


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

    void load_with_file(const char *path, bool reading);
};

Level *create_level(int level_num);
void destroy_level(Level *level);
