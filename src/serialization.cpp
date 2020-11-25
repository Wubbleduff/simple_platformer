
#include "serialization.h"

#include "platform.h"

struct Serialization::Stream
{
    int capacity;
    int size;
    char *data;
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

void Serialization::stream_write(Stream *stream, char a)
{
    int bytes = sizeof(a);

    maybe_grow_stream(stream, bytes);

    stream->data[stream->size] = a;
    stream->size += sizeof(a);
}

void Serialization::stream_write(Stream *stream, int a)
{
    int bytes = sizeof(a);

    maybe_grow_stream(stream, bytes);

    *(int *)(stream->data + stream->size) = a;
    stream->size += sizeof(a);
}

void Serialization::stream_read(Stream *stream, char *a)
{
    char *end = stream->data + stream->size;
    *a = *end;
    stream->size += sizeof(*a);
}

void Serialization::stream_read(Stream *stream, int *a)
{
    char *end = stream->data + stream->size;
    *a = *(int *)end;
    stream->size += sizeof(*a);
}



