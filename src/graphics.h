
#pragma once

#include "game_math.h"

struct Graphics
{
    static struct GraphicsState *instance;

    static void init();

    static void clear_frame(GameMath::v4 color);
    static void render();
    static void swap_frames();

    static void draw_quad(GameMath::v2 position, GameMath::v2 scale, float rotation, GameMath::v4 color);
    static void draw_circle(GameMath::v2 position, float radius, GameMath::v4 color);
    static void draw_line(GameMath::v2 a, GameMath::v2 b, GameMath::v4 color);
    static void draw_triangles(int num_vertices, GameMath::v2 *vertices, int num_triples, GameMath::v2 **triples, GameMath::v4 color);

    static GameMath::v2 ndc_point_to_world(GameMath::v2 ndc);

    static void set_camera_position(GameMath::v2 position);
    static void set_camera_width(float width);

    struct ImGuiImplementation
    {
        static void init();
        static void new_frame();
        static void end_frame();
        static void shutdown();
    };
};

