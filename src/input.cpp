
#include "input.h"
#include "platform.h"
#include "graphics.h"

#include <windows.h>
#include <assert.h>



static const int MAX_KEYS = 256;
static const int MAX_MOUSE_KEYS = 8;
struct InputState
{
    bool current_key_states[MAX_KEYS];
    bool previous_key_states[MAX_KEYS];
    bool event_key_states[MAX_KEYS]; // For recording windows key press events

    bool current_mouse_states[MAX_MOUSE_KEYS];
    bool previous_mouse_states[MAX_MOUSE_KEYS];
    bool event_mouse_states[MAX_MOUSE_KEYS]; // For recording windows key press events
};
InputState *Input::instance = nullptr;



// For recording windows key press events
void Input::record_key_event(int vk_code, bool state)
{
    instance->event_key_states[vk_code] = state;
}

// For recording windows key press events
void Input::record_mouse_event(int vk_code, bool state)
{
    instance->event_mouse_states[vk_code] = state;
}

void Input::read_input()
{
    // Read keyboard input
    memcpy(instance->previous_key_states, instance->current_key_states, sizeof(bool) * MAX_KEYS);
    memcpy(instance->current_key_states, instance->event_key_states, sizeof(bool) * MAX_KEYS);

    // Read mouse input
    memcpy(instance->previous_mouse_states, instance->current_mouse_states, sizeof(bool) * MAX_MOUSE_KEYS);
    memcpy(instance->current_mouse_states, instance->event_mouse_states, sizeof(bool) * MAX_MOUSE_KEYS);
}


bool Input::key_down(int key)
{
    assert(key < MAX_KEYS);
    return (instance->previous_key_states[key] == false && instance->current_key_states[key] == true) ? true : false;
}

bool Input::key(int key)
{
    assert(key < MAX_KEYS);
    return instance->current_key_states[key];
}


bool Input::mouse_button_down(int key)
{
    assert(key < MAX_MOUSE_KEYS);
    return (instance->previous_mouse_states[key] == false && instance->current_mouse_states[key] == true) ? true : false;
}

bool Input::mouse_button(int key)
{
    assert(key < MAX_MOUSE_KEYS);
    return instance->current_mouse_states[key];
}

v2 Input::mouse_world_position()
{
    v2 p = Platform::Window::mouse_screen_position();
    float screen_width = Platform::Window::screen_width();
    float screen_height = Platform::Window::screen_height();

    p.y = screen_height - p.y;

    v2 ndc =
    {
        (p.x / screen_width)  * 2.0f - 1.0f,
        (p.y / screen_height) * 2.0f - 1.0f,
    };

    return Graphics::ndc_point_to_world(ndc);
}



void Input::init()
{
    instance = (InputState *)Platform::Memory::allocate(sizeof(InputState));

    Platform::Memory::memset(instance->current_key_states,  0, sizeof(bool) * MAX_KEYS);
    Platform::Memory::memset(instance->previous_key_states, 0, sizeof(bool) * MAX_KEYS);
    Platform::Memory::memset(instance->event_key_states,    0, sizeof(bool) * MAX_KEYS);

    Platform::Memory::memset(instance->current_mouse_states,  0, sizeof(bool) * MAX_MOUSE_KEYS);
    Platform::Memory::memset(instance->previous_mouse_states, 0, sizeof(bool) * MAX_MOUSE_KEYS);
    Platform::Memory::memset(instance->event_mouse_states,    0, sizeof(bool) * MAX_MOUSE_KEYS);
}

