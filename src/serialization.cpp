
#include "serialization.h"

#include "platform.h"

struct Serialization::Stream
{
    int capacity;
    int size;
    char *data;
    int current_offset;
};

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

    free(old);
}



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

void Serialization::write_stream(Stream *stream, char a)
{
    int bytes = sizeof(a);

    maybe_grow_stream(stream, bytes);

    *(char *)(stream->data + stream->current_offset) = a;
    stream->size += sizeof(a);
    stream->current_offset += sizeof(a);
}

void Serialization::write_stream(Stream *stream, int a)
{
    int bytes = sizeof(a);

    maybe_grow_stream(stream, bytes);

    *(int *)(stream->data + stream->current_offset) = a;
    stream->size += sizeof(a);
    stream->current_offset += sizeof(a);
}

void Serialization::read_stream(Stream *stream, char *a)
{
    char *end = stream->data + stream->current_offset;
    *a = *(char *)end;
    stream->current_offset += sizeof(*a);
}

void Serialization::read_stream(Stream *stream, int *a)
{
    char *end = stream->data + stream->current_offset;
    *a = *(int *)end;
    stream->current_offset += sizeof(*a);
}



