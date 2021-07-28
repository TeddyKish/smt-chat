#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#define ARG_NUM 3

int main(int argc, char** argv) {

    //if (ARG_NUM != argc) {
        // Print help or something
    //} 

    //int server_ip = 0; //atoi(argv[1]);
    int server_port = 5000; //atoi(argv[2]);

    int chat_sock = socket(AF_INET, SOCK_STREAM, 0);

    // check that the socket was created

    struct sockaddr_in chat_server;
    memset(&chat_server, 0, sizeof(chat_server));

    chat_server.sin_family = AF_INET;
    chat_server.sin_addr.s_addr = inet_addr("127.0.0.1");
    chat_server.sin_port = server_port;

    if (0 > connect(chat_sock, (struct sockaddr*) &chat_server, sizeof(chat_server))) {
        //error
        perror("failed to connect");
        exit(1);
    }

    while(1){
        printf("ENTER MESSAGE:\n");
        char * message = malloc(5000*sizeof(char));

        scanf("%[^\n]",message);
        getchar();

        int sent = send(chat_sock , message, strlen(message), MSG_DONTWAIT);
        if (sent == -1) {
            printf("Failed to send");
        }
    }        

    return 0;
}