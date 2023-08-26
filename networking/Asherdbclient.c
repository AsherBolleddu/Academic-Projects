// Include necessary headers
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include "msg.h"

// Function prototypes
void handle_put(int sock);
void handle_get(int sock);

// Function to resolve hostname to IPv4 address
int resolve_hostname(const char *hostname, struct in_addr *addr)
{
    struct addrinfo hints, *result;
    int ret;

    // Initialize addrinfo structure with hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Perform DNS resolution using getaddrinfo
    if ((ret = getaddrinfo(hostname, NULL, &hints, &result)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    // Check if any IP address was found for the given hostname
    if (result == NULL)
    {
        fprintf(stderr, "No IP address found for the hostname: %s\n", hostname);
        return -1;
    }

    // Save the resolved IPv4 address and free the addrinfo structure
    *addr = ((struct sockaddr_in *)result->ai_addr)->sin_addr;
    freeaddrinfo(result);
    return 0;
}

// Main function
int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in server;
    int choice;

    // Check if the required arguments are provided
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create a socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Initialize sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));

    // Resolve hostname to an IPv4 address
    if (resolve_hostname(argv[1], &server.sin_addr) == -1)
    {
        perror("Error resolving hostname");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // Notify user about successful connection
    printf("Connected to server\n");

    // Main loop for user input
    while (1)
    {
        // Prompt user for a choice
        printf("Enter your choice (1 to put, 2 to get, 0 to quit): ");
        scanf("%d", &choice);

        // Process user's choice
        switch (choice)
        {
        case 1:
            handle_put(sock);
            break;
        case 2:
            handle_get(sock);
            break;
        case 0:
            close(sock);
            exit(EXIT_SUCCESS);
        default:
            printf("Invalid choice\n");
        }
    }

    return 0;
}

// Function to handle PUT operation
void handle_put(int sock)
{
    struct msg message;
    message.type = PUT;

    // Prompt user for input
    printf("Enter the name: ");
    getchar();
    fgets(message.rd.name, MAX_NAME_LENGTH, stdin);
    message.rd.name[strlen(message.rd.name) - 1] = '\0';

    printf("Enter the id: ");
    scanf("%u", &message.rd.id);

    // Send the message to the server
    if (send(sock, &message, sizeof(struct msg), 0) == -1)
    {
        perror("send");
        return;
    }

    // Receive the response from the server
    if (recv(sock, &message, sizeof(struct msg), 0) == -1)
    {
        perror("recv");
        return;
    }

    // Check if the operation was successful
    if (message.type == SUCCESS)
    {
        printf("Put success\n");
    }
    else
    {
        printf("Put failed\n");
    }
}

// Function to handle GET operation
void handle_get(int sock)
{
    struct msg message;
    message.type = GET;
    // Prompt user for input
    printf("Enter the id: ");
    scanf("%u", &message.rd.id);

    // Send the message to the server
    if (send(sock, &message, sizeof(struct msg), 0) == -1)
    {
        perror("send");
        return;
    }

    // Receive the response from the server
    if (recv(sock, &message, sizeof(struct msg), 0) == -1)
    {
        perror("recv");
        return;
    }

    // Check if the operation was successful
    if (message.type == SUCCESS)
    {
        printf("Name: %s\n", message.rd.name);
        printf("ID: %u\n", message.rd.id);
    }
    else
    {
        printf("Get failed\n");
    }
}