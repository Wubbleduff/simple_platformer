
#pragma once

#include "game_math.h"

struct Graphics
{
    static struct GraphicsState *instance;

    static void init();

    static void clear_frame(GameMath::v4 color);
    static void render();
    static void swap_frames();

    static void quad(GameMath::v2 position, GameMath::v2 scale, float rotation, GameMath::v4 color, int layer = 0);

    static GameMath::mat4 view_m_world();
    static GameMath::mat4 world_m_view();
    static GameMath::mat4 ndc_m_world();
    static GameMath::mat4 world_m_ndc();

    struct Camera
    {
        static struct CameraState *instance;

        static GameMath::v2 &position();
        static float &width();
    };

    struct ImGuiImplementation
    {
        static void init();
        static void new_frame();
        static void end_frame();
        static void shutdown();
    };
};

