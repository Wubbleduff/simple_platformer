
#include "network.h"
#include "logging.h"
#include "platform.h"
#include "data_structures.h"

#include <vector>
#include <WinSock2.h> // Networking API
#include <Ws2tcpip.h> // InetPton
#include <time.h>

#define CODE_WOULD_BLOCK WSAEWOULDBLOCK
#define CODE_IS_CONNECTED WSAEISCONN
#define CODE_INVALID WSAEINVAL

enum BlockingMode
{
    BLOCKING,
    NONBLOCKING
};

struct NetworkState
{
    SOCKET listening_socket;
};
NetworkState *Network::instance = nullptr;



static int get_last_error()
{
    return WSAGetLastError();
}

static bool read_bytes_from_socket(SOCKET socket, char *buffer, int max_bytes_to_read, int *bytes_actually_read)
{
    // Listen for a response
    long int bytes_read_from_socket = recv(socket, buffer, max_bytes_to_read, 0);

    *bytes_actually_read = (int)bytes_read_from_socket;

    // Check if bytes were read
    if(*bytes_actually_read == -1)
    {
        // Bytes weren't read, make sure there's an expected error code
        if(get_last_error() != CODE_WOULD_BLOCK)
        {
            Log::log_error("Error receiving data: %i\n", get_last_error());
        }
        return false;
    }
    else
    {
        // Bytes were read
        return true;
    }
}

static void set_blocking_mode(SOCKET socket, BlockingMode mode)
{
    u_long code = (mode == BLOCKING) ? 0 : 1;
    int error = ioctlsocket(socket, FIONBIO, &code);
    if(error == SOCKET_ERROR)
    {
        Log::log_error("Could not change socket blocking mode");
        return;
    }
}

static void init_winsock()
{
    int error;

    // Initialize Winsock
    WSADATA wsa_data;
    error = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if(error) Log::log_error("Error loading winsock: %i\n", get_last_error());
}

static void cleanup_winsock()
{
    // Shut down Winsock
    int error = WSACleanup();
    if(error == -1) Log::log_error("Error unloading networking module: %i\n", get_last_error());
}






void Network::init()
{
    instance = new NetworkState();

    init_winsock();

    instance->listening_socket = INVALID_SOCKET;
}


Network::Connection *Network::connect(const char *ip_address, int port)
{
    SOCKET new_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(new_socket == SOCKET_ERROR)
    {
        Log::log_error("Error opening socket: %i\n", get_last_error());
        return nullptr;
    }

    set_blocking_mode(new_socket, BLOCKING);

    // Create address
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    int error = inet_pton(AF_INET, ip_address, &address.sin_addr.s_addr);
    if(error == -1)
    {
        Log::log_error("Error creating an address: %i\n", error);
        return nullptr;
    }

    // Try to connect
    int code = ::connect(new_socket, (sockaddr *)(&address), sizeof(address));
    int status = get_last_error();

    // While not connected, try to reconnect
    while(code != 0 && status != CODE_IS_CONNECTED)
    {
        code = ::connect(new_socket, (sockaddr *)(&address), sizeof(address));
        status = get_last_error();

        if(status == CODE_INVALID)
        {
            error = CODE_INVALID;
            Log::log_error("Could not connect to %s:%i", ip_address, port);
            return nullptr;;
        }
    }

    set_blocking_mode(new_socket, NONBLOCKING);

    Connection *new_connection = Network::Connection::allocate_and_init_connection(new_socket, ip_address, port);

    Log::log_info("Connected to server %s:%i", ip_address, port);
    return new_connection;
}

void Network::disconnect(Network::Connection **connection)
{
    if(*connection == nullptr) return;

    set_blocking_mode((*connection)->tcp_socket, BLOCKING);

    int result = ::shutdown((*connection)->tcp_socket, SD_SEND);
    if(result == SOCKET_ERROR)
    {
        Log::log_error("Could not send shutdown signal");
    }

    // TODO: Have a timeout period
    static char buf[1024];
    while(true)
    {
        int received_bytes = 0;
        bool success = read_bytes_from_socket((*connection)->tcp_socket,
                buf, sizeof(buf), &received_bytes);
        if(!success) break;
        if(success && received_bytes == 0) break;
    }

    closesocket((*connection)->tcp_socket);

    Log::log_info("Disconnected from %s:%i", (*connection)->ip_address, (*connection)->port);

    //delete (*connection)->recorded_frames;
    delete[] (*connection)->receive_buffer;
    delete *connection;
    *connection = nullptr;
}

void Network::listen_for_client_connections(int port)
{
    // Create a listening socket
    SOCKET listening_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listening_socket == INVALID_SOCKET)
    {
        Log::log_error("Couldn't create listening socket: %d\n", get_last_error());
        return;
    }

    set_blocking_mode(listening_socket, NONBLOCKING);

    sockaddr_in bound_address;
    bound_address.sin_family = AF_INET;
    bound_address.sin_addr.s_addr = INADDR_ANY;
    bound_address.sin_port = htons(port);

    int error = bind(listening_socket, (SOCKADDR *)(&bound_address), sizeof(bound_address));
    if(error == SOCKET_ERROR)
    {
        error = get_last_error();
        Log::log_error("Couldn't bind listening socket to port: %d, error: %d\n", port, error);
        return;
    }

    error = listen(listening_socket, SOMAXCONN);
    if(error == SOCKET_ERROR)
    {
        Log::log_error("Couldn't listen on the created socket: %d\n", get_last_error());
        return;
    }

    instance->listening_socket = listening_socket;

    Log::log_info("Server listening on port %i...", port);
}

void Network::stop_listening_for_client_connections()
{
    closesocket(instance->listening_socket);
    instance->listening_socket = INVALID_SOCKET;
}

std::vector<Network::Connection *> Network::accept_client_connections()
{
    std::vector<Connection *> connections;

    int return_code = 0;
    do
    {
        sockaddr_in client_address;
        int client_address_size = sizeof(sockaddr_in);
        SOCKET incoming_socket = accept(instance->listening_socket, (sockaddr *)(&client_address), &client_address_size);
        if(incoming_socket == INVALID_SOCKET)
        {
            return_code = get_last_error();

            // NOTE:
            // This might fail if 2 servers are open on one port

            if(return_code != CODE_WOULD_BLOCK)
            {
                Log::log_error("Error accepting client: %d\n", get_last_error());
            }
            break;
        }

        // Valid socket
        char ip_address_string[NI_MAXHOST];
        char port_string[NI_MAXSERV];
        int rc = getnameinfo((struct sockaddr *)&client_address, client_address_size,
                ip_address_string, sizeof(ip_address_string),
                port_string, sizeof(port_string),
                NI_NUMERICHOST | NI_NUMERICSERV);
        int port_number = atoi(port_string);

        Connection *client_connection =
            Network::Connection::allocate_and_init_connection(incoming_socket, ip_address_string, port_number);

        connections.push_back(client_connection);

        time_t now;
        time(&now);
        Log::log_info("%s: Client connected on port %i", ctime(&now), client_connection->port);

    } while(return_code != CODE_WOULD_BLOCK);

    return connections;
}

void Network::Connection::send_stream(Serialization::Stream *stream)
{
    // Send data over TCP
    char *stream_data = stream->data();
    int bytes = stream->size();

    // TODO: Move this out
    char *send_buffer = new char[HEADER_SIZE + bytes]();
    Header header = { bytes };
    *(Header *)send_buffer = header;
    Platform::Memory::memcpy(send_buffer + HEADER_SIZE, stream_data, bytes);

    long int bytes_queued = send(tcp_socket, send_buffer, HEADER_SIZE + bytes, 0);
    if(bytes_queued == SOCKET_ERROR)
    {
        Log::log_error("Error sending data: %i\n", get_last_error());
    }

    if(bytes_queued != HEADER_SIZE + bytes)
    {
        Log::log_warning("Couldn't queue the requested number of bytes for sending. Requested: %i - Queued: %i",
                HEADER_SIZE + bytes, bytes_queued);
    }

    delete[] send_buffer;
}

Network::ReadResult Network::Connection::read_into_stream(Serialization::Stream *stream)
{
    if(!connected)
    {
        return Network::ReadResult::CLOSED;
    }

    // Update the connection data (read into buffer) and state (closed or still open)
    update_receive_state();

    // Check if the connection was closed after the read
    if(!connected)
    {
        Log::log_info("Connection to %s:%i closed", ip_address, port);
        return Network::ReadResult::CLOSED;
    }

    // Check if game data is ready to be read
    if(ready_to_read())
    {
        read_last_frame_into_stream(stream);
        return Network::ReadResult::READY;
    }
    else
    {
        return Network::ReadResult::NOT_READY;
    }
}




bool Network::Connection::expecting_header()
{
    char *content = receive_buffer + Connection::HEADER_SIZE;
    return (receive_target < content);
}

bool Network::Connection::data_frame_complete()
{
    if(expecting_header()) return false;

    char *buffer_end = receive_buffer + HEADER_SIZE + buffer_header->content_size;

    bool result = receive_target == buffer_end;
    return result;
}

void Network::Connection::add_data_frame(const char *frame)
{
    const Header *frame_header = (const Header *)frame;
    int new_frame_bytes = HEADER_SIZE + frame_header->content_size;
    char *new_frame = new char[new_frame_bytes]();
    Platform::Memory::memcpy(new_frame, frame, new_frame_bytes);
    recorded_frames.push_back(new_frame);
}

void Network::Connection::reset_receive_state()
{
    receive_target = receive_buffer;
}

void Network::Connection::update_receive_state()
{
    bool received_new_data = true;
    while(received_new_data)
    {
        // Recieve the data
        int receive_up_to_bytes;
        if(expecting_header())
        {
            receive_up_to_bytes = HEADER_SIZE;
        }
        else
        {
            receive_up_to_bytes = buffer_header->content_size;
        }

        int received_bytes;
        received_new_data = read_bytes_from_socket(tcp_socket,
                receive_target, receive_up_to_bytes, &received_bytes);

        if(received_new_data)
        {
            if(received_bytes == 0)
            {
                // Client closed the connection
                connected = false;
                return;
            }
            else
            {
                receive_target += received_bytes;

                if(data_frame_complete())
                {
                    add_data_frame(receive_buffer);
                    reset_receive_state();
                }
            }
        }
    }
}

bool Network::Connection::ready_to_read()
{
    return (recorded_frames.size() > 0);
}

void Network::Connection::read_last_frame_into_stream(Serialization::Stream *stream)
{
    char *frame = recorded_frames.back();
    Header *frame_header = (Header *)frame;

    int content_bytes = frame_header->content_size;
    char *content = frame + HEADER_SIZE;

    stream->write_array(content_bytes, content);

    delete[] frame;
    recorded_frames.pop_back();

    // Drop all previous frames for now...
    for(int i = 0; i < recorded_frames.size(); i++)
    {
        delete[] recorded_frames[i];
    }
    recorded_frames.clear();
}

Network::Connection *Network::Connection::allocate_and_init_connection(unsigned int in_socket, const char *ip_address, int port)
{
    Network::Connection *new_connection = new Network::Connection();

    new_connection->receive_buffer = new char[Network::Connection::RECEIVE_BUFFER_SIZE]();
    new_connection->receive_target = new_connection->receive_buffer;

    new_connection->tcp_socket = in_socket;
    strcpy(new_connection->ip_address, ip_address);
    new_connection->port = port;
    new_connection->connected = true;

    return new_connection;
}





