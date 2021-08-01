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
#define CLIENT_AMOUNT (10)

pthread_mutex_t fd_set_lock;
pthread_mutex_t client_mutexes[CLIENT_AMOUNT];
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

typedef struct message_info_s
{
    char* message;
    int sender_fd;
    int receiver_index;
} message_info_t;

typedef struct sockaddr_in socket_address_t;

void append_client_fd(int client_fd)
{
    int append_successful = 0;

    for (int i = 0; i < sizeof(client_descriptors); i++)
    {
        if (client_descriptors[i] == -1)
        {
            client_descriptors[i] = client_fd;
            append_successful = 1;
            break;
        }
        continue;
    }

    if (!append_successful)
    {
        perror("Too many clients connected to server");
        exit(1);
    }
}

void setup_server(server_info_t* server_info)
{
    // Initialize all variables and structures
    VALIDATE_RESULT(pthread_mutex_init(&fd_set_lock, NULL), "Failed initializing mutex");

    for (int i = 0; i < CLIENT_AMOUNT; i++)
    {
        VALIDATE_RESULT(pthread_mutex_init(&client_mutexes[i], NULL), "Failed initializing client mutex");
    }

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
    }
}

void* send_message(void* arg)
{
    
    message_info_t* message_info = (message_info_t*) arg;

    // Instead of waiting for all of the send_message threads to complete after every message, we can instead use mutexes.
    // With the introduction of mutexes for each message, we can guarantee that the server will never block.
    // However, the information may be received out-of-order in some clients (for example, several send_messages contending for the same mutex).
    //pthread_mutex_lock(&client_mutexes[message_info->receiver_index]);
    char s_num[6];
    snprintf(s_num, 6, "%d: ", message_info->sender_fd);
    write(client_descriptors[message_info->receiver_index], s_num, strnlen(s_num, BLOCK_SIZE - 1));
    write(client_descriptors[message_info->receiver_index], message_info->message, strnlen(message_info->message, BLOCK_SIZE - 1));
    //pthread_mutex_unlock(&client_mutexes[message_info->receiver_index]);
    
    free(message_info);

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

    return client_data;
}

int main(int argc, char const *argv[])
{
    fd_set current_sockets, ready_sockets;
    FD_ZERO(&current_sockets);

    pthread_t receive_thread, broadcast_threads[CLIENT_AMOUNT];
    int receive_ret, message_ret;

    server_info_t server_info;
    server_info.current_sockets = &current_sockets;
    server_info.port = 5000; 
    server_info.max_clients = 10;

    struct timeval second;
    setup_server(&server_info);

    receive_ret = pthread_create(&receive_thread, NULL, receive_new_connections, (void *)&server_info);
    
    while (1)
    {
        second.tv_sec = 1;
        second.tv_usec = 0;

        pthread_mutex_lock(&fd_set_lock);
        ready_sockets = current_sockets;
        pthread_mutex_unlock(&fd_set_lock);

        if (select(server_info.max_clients, &ready_sockets, NULL, NULL, &second) < 0 )
        {
            perror("Select failed");
            exit(1);
        }

        for (int i = 0; i < server_info.max_clients; i++)
        {
            if (FD_ISSET(i, &ready_sockets))
            {
                char* received_message = receive_message(i);

                for (int j = 0; i < sizeof(client_descriptors); j++)
                {
                    if (client_descriptors[j] == -1)
                    {
                        break;
                    }

                    if (client_descriptors[j] != i)
                    {
                        message_info_t* message_info = (message_info_t*)malloc(sizeof(message_info_t));
                        message_info->message = received_message;
                        message_info->sender_fd = i;
                        message_info->receiver_index = j;

                        pthread_create(&broadcast_threads[j], NULL, send_message, (void *)message_info);
                    }
                }

                // If we want to keep the server fully functional at all times, we may use mutexes instead of waiting for all threads
                for (int k = 0; k < sizeof(CLIENT_AMOUNT); k++)
                {
                    if (client_descriptors[k] != -1 && client_descriptors[k] != i)
                    {
                        pthread_join(broadcast_threads[k], NULL);
                    }
                }

                free(received_message);
            }
        }
    }
    
    return 0;
}