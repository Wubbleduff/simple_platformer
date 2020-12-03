
#include "logging.h"
#include "game_console.h"

#include "platform.h"
#include "data_structures.h"

#include <stdio.h>


struct LogState
{
    FILE *output_file;
};
LogState *Log::instance = nullptr;



static void log_message(LogState *instance, char *text)
{
    fprintf(instance->output_file, text);
    fflush(instance->output_file);

    GameConsole::write(text);
}



void Log::init()
{
    instance = (LogState *)Platform::Memory::allocate(sizeof(LogState));

    instance->output_file = fopen("output/game_log.txt", "w");
}

void Log::log_info_fn(const char *file, int line, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    const char *format1 = "-- INFO : %s : %i\n";
    const char *format2 = format;

    int format1_size = snprintf(nullptr, 0, format1, file, line);
    int format2_size = vsnprintf(nullptr, 0, format2, args) + 1;

    char *text_memory = (char *)Platform::Memory::allocate(format1_size + format2_size + 1);

    snprintf(text_memory, format1_size + 1, format1, file, line);
    vsnprintf(text_memory + format1_size, format2_size + 1, format2, args);
    text_memory[format1_size + format2_size - 1] = '\n';
    text_memory[format1_size + format2_size] = '\0';

    va_end(args);

    log_message(instance, text_memory);
}

void Log::log_warning_fn(const char *file, int line, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    const char *format1 = "-- WARNING : %s : %i\n";
    const char *format2 = format;

    int format1_size = snprintf(nullptr, 0, format1, file, line);
    int format2_size = vsnprintf(nullptr, 0, format2, args) + 1;

    char *text_memory = (char *)Platform::Memory::allocate(format1_size + format2_size + 1);

    snprintf(text_memory, format1_size + 1, format1, file, line);
    vsnprintf(text_memory + format1_size, format2_size + 1, format2, args);
    text_memory[format1_size + format2_size - 1] = '\n';
    text_memory[format1_size + format2_size] = '\0';

    va_end(args);

    log_message(instance, text_memory);
}

void Log::log_error_fn(const char *file, int line, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    const char *format1 = "-- ERROR : %s : %i\n";
    const char *format2 = format;

    int format1_size = snprintf(nullptr, 0, format1, file, line);
    int format2_size = vsnprintf(nullptr, 0, format2, args) + 1;

    char *text_memory = (char *)Platform::Memory::allocate(format1_size + format2_size + 1);

    snprintf(text_memory, format1_size + 1, format1, file, line);
    vsnprintf(text_memory + format1_size, format2_size + 1, format2, args);
    text_memory[format1_size + format2_size - 1] = '\n';
    text_memory[format1_size + format2_size] = '\0';

    va_end(args);

    log_message(instance, text_memory);
}
