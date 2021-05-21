
#pragma once

#include "game.h"
#include "game_math.h"
#include "serialization.h"

#include <map>



struct Levels
{
    static struct LevelsState *instance;

    static void init();
    static struct Level *create_level(int level_num);
    static void destroy_level(Level *level);
};

struct v2i
{
    union
    {
        struct { int x, y; };
        int v[2];
    };

    v2i() {}
    v2i(int a, int b): x(a), y(b) {}

    v2i operator+(v2i b) { return v2i(x + b.x, y + b.y); }
    v2i operator-(v2i b) { return v2i(x - b.x, y - b.y); }
    bool operator==(v2i b) { return (this->x == b.x) && (this->y == b.y); }
};

struct Level
{
public:
    

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
        v2i start_cell;

        void init();
        void clear();
        Cell *at(v2i pos);
        void set_filled(v2i pos, bool fill);
        GameMath::v2 cell_to_world(v2i pos);
        v2i world_to_cell(GameMath::v2 pos);
        void serialize(Serialization::Stream *stream, bool writing = true);
    };

    struct Editor
    {
        GameMath::v2 camera_position = GameMath::v2();
        GameMath::v2 last_mouse_position = GameMath::v2();

        char loaded_level[128];
        char text_input_buffer[128];
        char temp_input_buffer[128];

        union
        {
            struct
            {
                bool placing_start_point;
                bool placing_terrain;
                bool placing_win_points;
            };
            bool modes[3];
        };

        void step(Level *level, float time_step);
        void draw(Level *level);
        void draw_file_menu(Level *level);
    };

    int number;
    Grid grid;
    Editor editor;

    
    void init(int level_num);
    void init_default_level();
    void uninit();
    void clear();
    void step(float time_step);
    void draw();
    void serialize(Serialization::Stream *stream, bool writing);


private:
    void load_with_file(const char *path, bool writing);
};



