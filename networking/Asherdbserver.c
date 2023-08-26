// Include necessary header files
#include <arpa/inet.h>
#include <fcntl.h>  // for open
#include <unistd.h> // for close, write, read
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "msg.h"

// Define constant for database file
#define DATABASE_FILE "database.dat"

// Function prototype for handling client connections
void *handle_client(void *socket_desc);

// Main function
int main(int argc, char *argv[])
{
    // Declare variables
    int server_sock, client_sock;
    struct sockaddr_in server, client;
    socklen_t client_len;
    uint16_t port;

    // Check for correct number of arguments
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convert port from string to integer
    port = atoi(argv[1]);

    // Create server socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    // Bind server socket to address
    if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_sock, 10) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Print listening message
    printf("Server listening on port %d\n", port);

    // Accept incoming connections
    client_len = sizeof(struct sockaddr_in);
    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client, &client_len);
        if (client_sock == -1)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Create a new thread for each client connection
        pthread_t thread;
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;
        if (pthread_create(&thread, NULL, handle_client, (void *)new_sock) != 0)
        {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // Close server socket
    close(server_sock);
    return 0;
}

// Function to handle individual client connections
void *handle_client(void *arg)
{
    // Extract client socket from argument
    int client_sock = *((int *)arg);
    pthread_detach(pthread_self());
    free(arg);

    // Declare variables
    struct msg message;
    ssize_t bytes_received;
    ssize_t bytes_written;

    // Receive messages from client
    while ((bytes_received = recv(client_sock, &message, sizeof(struct msg), 0)) > 0)
    {
        // Open database file
        int database_fd = open(DATABASE_FILE, O_RDWR | O_CREAT, 0644);
        if (database_fd == -1)
        {
            perror("Failed to open the database file");
            close(client_sock);
            return NULL;
        }

        // Handle PUT message
        if (message.type == PUT)
        {
            off_t offset = sizeof(struct record) * message.rd.id;
            lseek(database_fd, offset, SEEK_SET);
            bytes_written = write(database_fd, &message.rd, sizeof(struct record));
            // Check if write was successful
            if (bytes_written == sizeof(struct record))
            {
                message.type = SUCCESS;
            }
            else
            {
                message.type = FAIL;
            }
        }
        // Handle GET message
        else if (message.type == GET)
        {
            off_t offset = sizeof(struct record) * message.rd.id;
            lseek(database_fd, offset, SEEK_SET);
            bytes_received = read(database_fd, &message.rd, sizeof(struct record));
            // Check if read was successful and record exists
            if (bytes_received == sizeof(struct record) && message.rd.name[0] != '\0')
            {
                message.type = SUCCESS;
            }
            else
            {
                message.type = FAIL;
            }
        }
        // Close database file
        close(database_fd);
        // Send response message to the client
        send(client_sock, &message, sizeof(struct msg), 0);
    }

    // Close client socket
    close(client_sock);
    return NULL;
}