
#pragma once

#include "my_math.h"
#include <windows.h>

struct PlatformState;

PlatformState *init_platform();

void platform_events(PlatformState *platform_state);

bool want_to_close(PlatformState *platform_state);

HWND get_window_handle(PlatformState *platform_state);
HDC get_device_context(PlatformState *platform_state);
int get_screen_width(PlatformState *platform_state);
int get_screen_height(PlatformState *platform_state);
int get_monitor_frequency(PlatformState *platform_state);
float get_aspect_ratio(PlatformState *platform_state);
v2 mouse_screen_position(PlatformState *platform_state);

double time_since_start(PlatformState *platform_state);



char *read_file_as_string(const char *path);



#define log_info(format, ...) log_info_fn(__FILE__, __LINE__, format, __VA_ARGS__);
void log_info_fn(const char *file, int line, const char *format, ...);

#define log_warning(format, ...) log_warning_fn(__FILE__, __LINE__, format, __VA_ARGS__);
void log_warning_fn(const char *file, int line, const char *format, ...);

#define log_error(format, ...) log_error_fn(__FILE__, __LINE__, format, __VA_ARGS__);
void log_error_fn(const char *file, int line, const char *format, ...);

