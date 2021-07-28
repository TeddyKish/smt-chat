#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BLOCK_SIZE (1024)

pthread_mutex_t fd_set_lock;
int client_descriptors[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};


#define VALIDATE_RESULT(func_result, message) \
        if (func_result != 0) { \
            perror(message); \
            exit(func_result); \
        }

typedef struct server_info_s
{
    fd_set* current_sockets;
    int socket;
    uint16_t port;
    uint8_t max_clients;
} server_info_t;

typedef struct sockaddr_in socket_address_t;

void append_client_fd(int client_fd)
{
    for (int i = 0; i < sizeof(client_descriptors); i++)
    {
        if (client_descriptors[i] == -1)
        {
            client_descriptors[i] = client_fd;
            break;
        }
        continue;
    }
}

void setup_server(server_info_t* server_info)
{
    socket_address_t server_address;
    server_info->socket = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_info->port);

    VALIDATE_RESULT(bind(server_info->socket, (socket_address_t*) &server_address, sizeof(server_address)), "Bind failed");
    VALIDATE_RESULT(listen(server_info->socket, server_info->max_clients), "Listen failed");

    printf("Server was set-up successfully\n");
}

void* receive_new_connections(void* arg)
{
    server_info_t* server_info = (server_info_t*)arg;
    int connection_fd = 0;
    
    printf("Starting to receive connections...\n");

    while (1)
    {
        if ((connection_fd = accept(server_info->socket, NULL, NULL)) == -1)
        {
            perror("Accept failed");
            exit(1);
        }
        
        pthread_mutex_lock(&fd_set_lock);
        FD_SET(connection_fd, server_info->current_sockets);
        append_client_fd(connection_fd);
        pthread_mutex_unlock(&fd_set_lock);

        printf("SERVER: New user has joined the chat!\n");
        // pthread send message new user joined the chat
    }
}

void* broadcast_message(void* arg)
{
    // send a message to all clients
    char* message = (char*) arg;
    for (int i = 0; i < sizeof(client_descriptors); i++)
    {
        if (client_descriptors[i] == -1)
        {
            break;
        }

        write(client_descriptors[i], message, strnlen(message, BLOCK_SIZE - 1));
    }

    printf("Successfully broadcasted the message: %s\n", message);
    free(message);

    return NULL;
}

char* receive_message(int client_fd)
{
    char* client_data = (char*)calloc(BLOCK_SIZE, sizeof(char));
    uint16_t bytes_read = read(client_fd, client_data, BLOCK_SIZE - 1);
    if (bytes_read == 0)
    {
        perror("Reading from user connection failed");
        exit(1);
    }

    printf("Successfully received a message from a client\n");

    return client_data;
}

int main(int argc, char const *argv[])
{
    // Initialize all variables and structures
    VALIDATE_RESULT(pthread_mutex_init(&fd_set_lock, NULL), "Failed initializing mutex");
    
    fd_set current_sockets, ready_sockets;
    FD_ZERO(&current_sockets);

    pthread_t receive_thread, message_thread, broadcast_thread;
    int receive_ret, message_ret;

    server_info_t* server_info = (server_info_t*)(malloc(sizeof(server_info)));
    server_info->current_sockets = &current_sockets;
    server_info->port = 5000;// parsed from argv server_info->port = port;
    server_info->max_clients = 10;// parsed from argv server_info->max_clients = max_clients;

    struct timeval second;
    

    setup_server(server_info);

    receive_ret = pthread_create(&receive_thread, NULL, receive_new_connections, (void *)server_info);
    
    while (1)
    {
        second.tv_sec = 1;
        second.tv_usec = 0;

        pthread_mutex_lock(&fd_set_lock);
        ready_sockets = current_sockets;
        pthread_mutex_unlock(&fd_set_lock);

        if (select(server_info->max_clients, &ready_sockets, NULL, NULL, &second) < 0 ) // later introduce timeout here
        {
            perror("Select failed");
            exit(1);
        }

        for (int i = 0; i < server_info->max_clients; i++)
        {
            if (FD_ISSET(i, &ready_sockets))
            {
                char* received_message = receive_message(i);
                printf("Received message: %s\n", received_message);
                pthread_create(&broadcast_thread, NULL, broadcast_message, (void *)received_message);
                pthread_join(broadcast_thread, NULL);
            }
        }
    }
    
   free(server_info);

    return 0;
}