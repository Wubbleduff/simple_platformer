
#pragma once

#include "my_math.h"

struct Graphics
{
    static struct GraphicsState *instance;

    static void init();

    static void clear_frame(v4 color);
    static void render();
    static void swap_frames();

    static void draw_quad(v2 position, v2 scale, float rotation, v4 color);
    static void draw_circle(v2 position, float radius, v4 color);
    static void draw_line(v2 a, v2 b, v4 color);
    static void draw_triangles(int num_vertices, v2 *vertices, int num_triples, v2 **triples, v4 color);

    static v2 ndc_point_to_world(v2 ndc);

    static void set_camera_position(v2 position);
    static void set_camera_width(float width);

    struct ImGuiImplementation
    {
        static void init();
        static void new_frame();
        static void end_frame();
    };
};

