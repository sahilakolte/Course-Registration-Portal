#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

#define PORT 8080
#define MAX 1024

void error (const char* msg) {
    perror(msg);
    exit(1);
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[MAX];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(0);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        perror("Connection with the server failed");
        exit(0);
    }

    printf("Connected to the Academia Portal Server.\n");

    while (1) {
        bzero(buffer, sizeof(buffer));
        read(sockfd, buffer, sizeof(buffer));
        printf("%s", buffer);
        fflush(stdout);

        if (strstr(buffer, "Invalid Credentials\n") != NULL) {
            break;
        }

        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        write(sockfd, buffer, strlen(buffer));

        if (strstr(buffer, "exit") != NULL) {
            break;
        }
        bzero(buffer, sizeof(buffer));
    }

    close(sockfd);
    return 0;
}