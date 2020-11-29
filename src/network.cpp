
#include "network.h"
#include "input.h"

#include "platform.h"

struct Connection
{

};

struct NetworkState
{
    struct ServerState
    {
        int num_client_connections;
        Connection *client_connections;
    } server;
};
NetworkState *Network::instance = nullptr;



static void read_into_stream(Connection *connection, Serialization::Stream *stream)
{

}



void Network::init()
{
    instance = (NetworkState *)Platform::Memory::allocate(sizeof(NetworkState));
}

// Client
void Network::send_input_state_to_server()
{
    /*
    Serialization::Stream *stream = Serialization::make_stream();

    Input::serialize_input_state(stream);

    send_stream(instance->server_connection, stream);

    Serialization::free_stream(stream);
    */
}

void Network::read_game_state(Serialization::Stream *game_state_stream)
{
    /*
    read_into_stream(instance->server_connection, stream);
    */
}



// Server
void Network::read_client_input_states()
{
    Serialization::Stream *stream = Serialization::make_stream();

    for(int i = 0; i < instance->server.num_client_connections; i++)
    {
        Connection *client_connection = &(instance->server.client_connections[i]);

        Serialization::reset_stream(stream);

        read_into_stream(client_connection, stream);

        Input::deserialize_input_state(stream);
    }

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

