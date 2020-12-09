
#pragma once

#include "game_math.h"


struct Serialization
{
    struct Stream
    {
        int capacity;
        int stream_size;
        char *stream_data;
        int current_offset;

        char *data();
        int size();
        bool at_end();
        void move_to_beginning();
        void clear();
        void write(char item);
        void write(int item);
        void write(float item);
        void write(GameMath::v2 item);
        void write(GameMath::v3 item);
        void write(GameMath::v4 item);
        void write_array(int num, char *array);
        void read(char *item);
        void read(int *item);
        void read(float *item);
        void read(GameMath::v2 *item);
        void read(GameMath::v3 *item);
        void read(GameMath::v4 *item);
        void read_array(int num, char *array);
    };

    static Stream *make_stream(int capacity = 1);
    static void free_stream(Stream *stream);
};

