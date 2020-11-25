
#include "serialization.h"

#include "platform.h"

struct Serialization::Stream
{
    int capacity;
    int size;
    char *data;
};

static void maybe_grow_stream(Serialization::Stream **stream, int bytes)
{
    if((*stream)->capacity > bytes)
    {
        return;
    }

    int capacity = (*stream)->capacity;
    while(capacity <= bytes)
    {
        capacity *= 2;
    }

    Serialization::Stream *old = *stream;
    *stream = (Serialization::Stream *)Platform::Memory::allocate(sizeof(Serialization::Stream) + capacity);

    (*stream)->capacity = capacity;
    (*stream)->size = old->size;
    (*stream)->data = (char *)(*stream) + sizeof(Serialization::Stream);

    Platform::Memory::memcpy((*stream)->data, old->data, (*stream)->size);
}



Serialization::Stream *Serialization::make_stream(int capacity)
{
    Stream *stream = (Stream *)Platform::Memory::allocate(sizeof(Stream) + capacity);
    stream->capacity = capacity;
    stream->size = 0;
    stream->data = (char *)stream + sizeof(stream);
    return stream;
}

void Serialization::free_stream(Stream *stream)
{
    Platform::Memory::free(stream);
}

void Serialization::stream_write(Stream *stream, char a)
{
    int bytes = sizeof(a);

    maybe_grow_stream(&stream, bytes);

    stream->data[stream->size] = a;
    stream->size += sizeof(a);
}

void Serialization::stream_write(Stream *stream, int a)
{
    int bytes = sizeof(a);

    maybe_grow_stream(&stream, bytes);

    *(int *)(stream->data + stream->size) = a;
    stream->size += sizeof(a);
}

char *Serialization::stream_data(Stream *stream)
{
    return stream->data;
}

int Serialization::stream_size(Stream *stream)
{
    return stream->size;
}


