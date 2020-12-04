
#pragma once

#include "game_math.h"

struct Serialization
{
    struct Stream;

    static Stream *make_stream(int capacity = 1);
    static void free_stream(Stream *stream);

    static char *stream_data(Stream *stream);
    static int stream_size(Stream *stream);

    static void clear_stream(Stream *stream);
    static void reset_stream(Stream *stream);

    static void write_stream(Stream *stream, char item);
    static void write_stream(Stream *stream, int item);
    static void write_stream(Stream *stream, float item);
    static void write_stream(Stream *stream, GameMath::v2 item);
    static void write_stream(Stream *stream, GameMath::v3 item);
    static void write_stream(Stream *stream, GameMath::v4 item);
    static void write_stream_array(Stream *stream, int num, char *array);

    static void read_stream(Stream *stream, char *item);
    static void read_stream(Stream *stream, int *item);
    static void read_stream(Stream *stream, float *item);
    static void read_stream(Stream *stream, GameMath::v2 *item);
    static void read_stream(Stream *stream, GameMath::v3 *item);
    static void read_stream(Stream *stream, GameMath::v4 *item);
    static void read_stream_array(Stream *stream, int num, char *array);

};

