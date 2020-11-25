
#pragma once

#include "my_math.h"
#include <windows.h>

struct Platform
{
    static struct PlatformState *instance;

    static void init();
    static void handle_os_events();
    static bool want_to_close();
    static double time_since_start();

    // Window application
    struct Window
    {
        static HWND handle();
        static HDC device_context();
        static int screen_width();
        static int screen_height();
        static int monitor_frequency();
        static float aspect_ratio();
        static v2 mouse_screen_position();
    };

    // Memory management
    struct Memory
    {
        static void *allocate(int bytes);
        static void free(void *data);
        static void memset(void *buffer, int value, int bytes);
        static void memcpy(void *dest, void *src, int bytes);
    };

    // File managment
    struct File;
    struct FileSystem
    {
        enum FileMode
        {
            READ,
            WRITE,
            READ_WRITE
        };

        static File *open(const char *path, FileMode mode);
        static void close(File *file);
        static void read(File *file, void *buffer, int bytes);
        static void write(File *file, void *buffer, int bytes);
        static int size(File *file);
        static char *read_file_into_string(const char *path);
    };


#define log_info(format, ...) log_info_fn(__FILE__, __LINE__, format, __VA_ARGS__);
    static void log_info_fn(const char *file, int line, const char *format, ...);

#define log_warning(format, ...) log_warning_fn(__FILE__, __LINE__, format, __VA_ARGS__);
    static void log_warning_fn(const char *file, int line, const char *format, ...);

#define log_error(format, ...) log_error_fn(__FILE__, __LINE__, format, __VA_ARGS__);
    static void log_error_fn(const char *file, int line, const char *format, ...);

};

