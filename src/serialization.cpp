
#include "serialization.h"

#include "platform.h"



struct Serialization::Stream
{
    int capacity;
    int size;
    char *data;
    int current_offset;
};




Serialization::Stream *Serialization::make_stream(int capacity)
{
    Stream *stream = (Stream *)Platform::Memory::allocate(sizeof(Stream));
    stream->capacity = capacity;
    stream->size = 0;
    stream->data = (char *)Platform::Memory::allocate(capacity);
    stream->current_offset = 0;
    return stream;
}

void Serialization::free_stream(Stream *stream)
{
    Platform::Memory::free(stream);
}

char *Serialization::stream_data(Stream *stream)
{
    return stream->data;
}

int Serialization::stream_size(Stream *stream)
{
    return stream->size;
}

void Serialization::clear_stream(Stream *stream)
{
    stream->size = 0;
    stream->current_offset = 0;
}

void Serialization::reset_stream(Stream *stream)
{
    stream->current_offset = 0;
}




static void maybe_grow_stream(Serialization::Stream *stream, int bytes)
{
    int new_size = stream->size + bytes;
    if(stream->capacity > new_size) return;

    int new_capacity = stream->capacity;
    while(new_capacity <= new_size)
    {
        new_capacity *= 2;
    }

    char *old = stream->data;
    stream->data = (char *)Platform::Memory::allocate(new_capacity);

    stream->capacity = new_capacity;

    Platform::Memory::memcpy(stream->data, old, stream->size);

    Platform::Memory::free(old);
}

template<typename T>
static void write_stream_t(Serialization::Stream *stream, T item)
{
    int bytes = sizeof(item);

    maybe_grow_stream(stream, bytes);

    *(T *)(stream->data + stream->current_offset) = item;
    stream->size += bytes;
    stream->current_offset += bytes;
}


template<typename T>
static void write_stream_array_t(Serialization::Stream *stream, int num, T *array)
{
    int bytes = num * sizeof(*array);

    maybe_grow_stream(stream, bytes);

    Platform::Memory::memcpy(stream->data + stream->current_offset, array, bytes);

    stream->size += bytes;
    stream->current_offset += bytes;
}

template<typename T>
static void read_stream_t(Serialization::Stream *stream, T *item)
{
    char *end = stream->data + stream->current_offset;
    *item = *(T *)end;
    stream->current_offset += sizeof(*item);
}

template<typename T>
static void read_stream_array_t(Serialization::Stream *stream, int num, T *array)
{
    char *end = stream->data + stream->current_offset;
    int bytes = num * sizeof(*array);
    Platform::Memory::memcpy(array, end, bytes);
    stream->current_offset += bytes;
}



void Serialization::write_stream(Stream *stream, char item)
{
    write_stream_t(stream, item);
}
void Serialization::write_stream(Stream *stream, int item)
{
    write_stream_t(stream, item);
}
void Serialization::write_stream(Stream *stream, float item)
{
    write_stream_t(stream, item);
}
void Serialization::write_stream(Stream *stream, GameMath::v2 item)
{
    write_stream_t(stream, item);
}
void Serialization::write_stream(Stream *stream, GameMath::v3 item)
{
    write_stream_t(stream, item);
}
void Serialization::write_stream(Stream *stream, GameMath::v4 item)
{
    write_stream_t(stream, item);
}
void Serialization::write_stream_array(Stream *stream, int num, char *array)
{
    write_stream_array_t(stream, num, array);
}

void Serialization::read_stream(Stream *stream, char *item)
{
    read_stream_t(stream, item);
}
void Serialization::read_stream(Stream *stream, int *item)
{
    read_stream_t(stream, item);
}
void Serialization::read_stream(Stream *stream, float *item)
{
    read_stream_t(stream, item);
}
void Serialization::read_stream(Stream *stream, GameMath::v2 *item)
{
    read_stream_t(stream, item);
}
void Serialization::read_stream(Stream *stream, GameMath::v3 *item)
{
    read_stream_t(stream, item);
}
void Serialization::read_stream(Stream *stream, GameMath::v4 *item)
{
    read_stream_t(stream, item);
}
void Serialization::read_stream_array(Stream *stream, int num, char *array)
{
    read_stream_array_t(stream, num, array);
}


