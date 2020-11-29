
#include "network.h"
#include "input.h"
#include "platform.h"

#include <WinSock2.h> // Networking API
#include <Ws2tcpip.h> // InetPton

#define CODE_WOULD_BLOCK WSAEWOULDBLOCK
#define CODE_IS_CONNECTED WSAEISCONN
#define CODE_INVALID WSAEINVAL

struct Connection
{

};

struct NetworkState
{
    struct ClientState
    {
        Connection server_connection;
    };
    ClientState client;

    struct ServerState
    {
        static const int MAX_CLIENTS = 8;
        int num_client_connections;
        Connection *client_connections;
    };
    ServerState server;

};
NetworkState *Network::instance = nullptr;



static void send_stream(Connection *connection, Serialization::Stream *stream)
{
}

static void read_into_stream(Connection *connection, Serialization::Stream *stream)
{
}



void Network::init()
{
    instance = (NetworkState *)Platform::Memory::allocate(sizeof(NetworkState));

    // Client
    {
        instance->client.server_connection = {};
    }

    // Server
    {
        instance->server.num_client_connections = 0;
        instance->server.client_connections =
            (Connection *)Platform::Memory::allocate(sizeof(Connection) * NetworkState::ServerState::MAX_CLIENTS);
    }
}

// Client
void Network::send_input_state_to_server()
{
    Serialization::Stream *stream = Serialization::make_stream();

    Input::serialize_input_state(stream);

    send_stream(&(instance->client.server_connection), stream);

    Serialization::free_stream(stream);
}

void Network::read_game_state(Serialization::Stream *stream)
{
    read_into_stream(&(instance->client.server_connection), stream);
}



// Server
void Network::read_client_input_states()
{
    Serialization::Stream *stream = Serialization::make_stream();

    for(int i = 0; i < instance->server.num_client_connections; i++)
    {
        Connection *client_connection = &(instance->server.client_connections[i]);

        Serialization::clear_stream(stream);
        read_into_stream(client_connection, stream);
        Serialization::reset_stream(stream);
        Input::deserialize_input_state(stream);
    }

    // DEBUG
#if 0
    {
        static int count = 0;
        count++;
        for(int i = 0; i < 256; i++)
        {
            int value = 0;
            if(i == 'D') value = 1;
            if(i == ' ') value = 1;
            Serialization::write_stream(stream, value);
        }
        for(int i = 0; i < 256; i++) Serialization::write_stream(stream, 0);
        for(int i = 0; i < 8; i++) Serialization::write_stream(stream, 0);
        for(int i = 0; i < 8; i++) Serialization::write_stream(stream, 0);

        Serialization::reset_stream(stream);

        Input::deserialize_input_state(stream);
    }
#endif

    Serialization::free_stream(stream);
}

void Network::broadcast_game_state(Serialization::Stream *game_state_stream)
{
    /*
    for(int i = 0; i < instance->num_client_connections; i++)
    {
        send_stream(instance->client_connections[i], game_state_stream)
    }
    */
}

