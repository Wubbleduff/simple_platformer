
#pragma once

struct Serialization
{
    struct Stream;

    static Stream *make_stream(int capacity = 1);
    static void free_stream(Stream *stream);

    static char *stream_data(Stream *stream);
    static int stream_size(Stream *stream);

    static void clear_stream(Stream *stream);
    static void reset_stream(Stream *stream);

    static void write_stream(Stream *stream, char a);
    static void write_stream(Stream *stream, int a);

    static void read_stream(Stream *stream, char *a);
    static void read_stream(Stream *stream, int *a);
};

