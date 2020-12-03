
#pragma once

#include <cstdarg>

struct Log
{
    static struct LogState *instance;

    static void init();

#define log_info(format, ...) log_info_fn(__FILE__, __LINE__, format, __VA_ARGS__);
    static void log_info_fn(const char *file, int line, const char *format, ...);

#define log_warning(format, ...) log_warning_fn(__FILE__, __LINE__, format, __VA_ARGS__);
    static void log_warning_fn(const char *file, int line, const char *format, ...);

#define log_error(format, ...) log_error_fn(__FILE__, __LINE__, format, __VA_ARGS__);
    static void log_error_fn(const char *file, int line, const char *format, ...);
};

