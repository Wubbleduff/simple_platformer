
#pragma once

#include "serialization.h"
#include "data_structures.h"

#include <vector>

struct Network
{
    static struct NetworkState *instance;

    enum class ReadResult
    {
        CLOSED,
        READY,
        NOT_READY
    };

    struct Connection
    {
    public:
        void send_stream(Serialization::Stream *stream);
        ReadResult read_into_stream(Serialization::Stream *stream);



    private:
        bool connected;
        //SOCKET tcp_socket;
        unsigned int tcp_socket;
        char ip_address[16];
        int port;
        struct Header
        {
            int content_size;
        };
        static const int RECEIVE_BUFFER_SIZE = 1024 * 2;
        union
        {
            char *receive_buffer;
            Header *buffer_header;
        };
        char *receive_target;
        static const int HEADER_SIZE = sizeof(Header);
        std::vector<char *> recorded_frames;

        bool expecting_header();
        bool data_frame_complete();
        void add_data_frame(const char *frame);
        void reset_receive_state();
        void update_receive_state();
        bool ready_to_read();
        void read_last_frame_into_stream(Serialization::Stream *stream);

        friend class Network;
        static Connection *allocate_and_init_connection(unsigned int in_socket, const char *ip_address, int port);

    };

    static void init();

    static Connection *connect(const char *ip_address, int port);
    static void disconnect(Connection **connection);

    static void listen_for_client_connections(int port);
    static void stop_listening_for_client_connections();
    static std::vector<Connection *> accept_client_connections();
};

