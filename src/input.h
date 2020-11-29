
#pragma once

#include "my_math.h"
#include "serialization.h"

struct Input
{
    static struct InputState *instance;

    static void init();

    static bool key_down(int button);
    static bool key_up(int button);
    static bool key(int button);

    static bool mouse_button_down(int button);
    static bool mouse_button_up(int button);
    static bool mouse_button(int button);

    static v2 mouse_world_position();

    static void read_input();
    static void record_key_event(int vk_code, bool state);
    static void record_mouse_event(int vk_code, bool state);

    static void serialize_input_state(Serialization::Stream *stream);
    static void deserialize_input_state(Serialization::Stream *stream);
};


