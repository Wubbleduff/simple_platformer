
#pragma once

struct Serialization
{
    struct Stream;

    static Stream *make_stream(int capacity = 1);
    static void free_stream(Stream *stream);

    static char *stream_data(Stream *stream);
    static int stream_size(Stream *stream);

    static void reset_stream(Stream *stream);

    static void stream_write(Stream *stream, char a);
    static void stream_write(Stream *stream, int a);

    static void stream_read(Stream *stream, char *a);
    static void stream_read(Stream *stream, int *a);
};

