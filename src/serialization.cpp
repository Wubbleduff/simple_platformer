
#include "serialization.h"

#include "platform.h"
#include "logging.h"



Serialization::Stream *Serialization::make_stream(int capacity)
{
    Stream *stream = new Stream();
    stream->capacity = capacity;
    stream->stream_size = 0;
    stream->stream_data = new char[capacity]();
    stream->current_offset = 0;
    return stream;
}

Serialization::Stream *Serialization::make_stream_from_file(const char *path)
{
    Platform::File *file = Platform::FileSystem::open(path, Platform::FileSystem::READ);
    if(!file)
    {
        //Log::log_error("Tried to make a stream from a non-existent file: %s", path);
        return nullptr;
    }

    int file_size = Platform::FileSystem::size(file);

    Stream *result = make_stream(file_size);
    Platform::FileSystem::read(file, result->data(), file_size);

    Platform::FileSystem::close(file);

    return result;
}

void Serialization::free_stream(Stream *stream)
{
    delete[] stream->stream_data;
    delete stream;
}

char *Serialization::Stream::data()
{
    return stream_data;
}

int Serialization::Stream::size()
{
    return stream_size;
}

bool Serialization::Stream::at_end()
{
    //assert(current_offset <= size);
    return current_offset == stream_size;
}

void Serialization::Stream::clear()
{
    stream_size = 0;
    current_offset = 0;
}

void Serialization::Stream::move_to_beginning()
{
    current_offset = 0;
}




static void maybe_grow_stream(Serialization::Stream *stream, int bytes)
{
    int new_size = stream->stream_size + bytes;
    if(stream->capacity > new_size) return;

    int new_capacity = stream->capacity;
    while(new_capacity <= new_size)
    {
        new_capacity *= 2;
    }

    char *old = stream->stream_data;
    stream->stream_data = new char[new_capacity]();

    stream->capacity = new_capacity;

    Platform::Memory::memcpy(stream->stream_data, old, stream->stream_size);

    delete[] old;
}

template<typename T>
static void write_stream_t(Serialization::Stream *stream, T item)
{
    int bytes = sizeof(item);

    maybe_grow_stream(stream, bytes);

    *(T *)(stream->stream_data + stream->current_offset) = item;
    stream->stream_size += bytes;
    stream->current_offset += bytes;
}


template<typename T>
static void write_stream_array_t(Serialization::Stream *stream, int num, T *array)
{
    int bytes = num * sizeof(*array);

    maybe_grow_stream(stream, bytes);

    Platform::Memory::memcpy(stream->stream_data + stream->current_offset, array, bytes);

    stream->stream_size += bytes;
    stream->current_offset += bytes;
}

template<typename T>
static void read_stream_t(Serialization::Stream *stream, T *item)
{
    char *end = stream->stream_data + stream->current_offset;
    *item = *(T *)end;
    stream->current_offset += sizeof(*item);
}

template<typename T>
static void read_stream_array_t(Serialization::Stream *stream, int num, T *array)
{
    char *end = stream->stream_data + stream->current_offset;
    int bytes = num * sizeof(*array);
    Platform::Memory::memcpy(array, end, bytes);
    stream->current_offset += bytes;
}



void Serialization::Stream::write(char item)
{
    write_stream_t(this, item);
}
void Serialization::Stream::write(int item)
{
    write_stream_t(this, item);
}
void Serialization::Stream::write(unsigned int item)
{
    write_stream_t(this, item);
}
void Serialization::Stream::write(float item)
{
    write_stream_t(this, item);
}
void Serialization::Stream::write(GameMath::v2 item)
{
    write_stream_t(this, item);
}
void Serialization::Stream::write(GameMath::v3 item)
{
    write_stream_t(this, item);
}
void Serialization::Stream::write(GameMath::v4 item)
{
    write_stream_t(this, item);
}
void Serialization::Stream::write_array(int num, char *array)
{
    write_stream_array_t(this, num, array);
}

void Serialization::Stream::read(char *item)
{
    read_stream_t(this, item);
}
void Serialization::Stream::read(int *item)
{
    read_stream_t(this, item);
}
void Serialization::Stream::read(unsigned int *item)
{
    read_stream_t(this, item);
}
void Serialization::Stream::read(float *item)
{
    read_stream_t(this, item);
}
void Serialization::Stream::read(GameMath::v2 *item)
{
    read_stream_t(this, item);
}
void Serialization::Stream::read(GameMath::v3 *item)
{
    read_stream_t(this, item);
}
void Serialization::Stream::read(GameMath::v4 *item)
{
    read_stream_t(this, item);
}
void Serialization::Stream::read_array(int num, char *array)
{
    read_stream_array_t(this, num, array);
}

void Serialization::Stream::write_to_file(const char *path)
{
    Platform::File *file = Platform::FileSystem::open(path, Platform::FileSystem::WRITE);
    Platform::FileSystem::write(file, data(), size());
    Platform::FileSystem::close(file);
}


