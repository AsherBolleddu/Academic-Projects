#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "msg.h"
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <inttypes.h>

#define DB "data.txt"

void Usage(char *progname);
void PrintOut(int fd, struct sockaddr *addr, size_t addrlen);
void PrintReverseDNS(struct sockaddr *addr, size_t addrlen);
void PrintServerSide(int client_fd, int sock_family);

int Listen(char *portnum, int *sock_family);
void *HandleClient(void *arg);

// introduced a struct to circumvent arg passing limitations of pthread_create
struct handlerParam
{
    struct sockaddr_storage caddr;
    socklen_t caddr_len;
    int client_fd;
};

int main(int argc, char **argv)
{
    // Expect the port number as a command line argument.
    if (argc != 2)
    {
        Usage(argv[0]);
    }

    int sock_family;
    int listen_fd = Listen(argv[1], &sock_family);
    if (listen_fd <= 0)
    {
        // We failed to bind/listen to a socket.  Quit with failure.
        printf("Couldn't bind to any addresses.\n");
        return EXIT_FAILURE;
    }

    // Loop forever, accepting a connection from a client and doing
    // an echo trick to it.
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    if (getsockname(listen_fd, (struct sockaddr *)&addr, &addrlen) == 0)
    {
        printf("Server listening on: ");
        PrintOut(listen_fd, (struct sockaddr *)&addr, addrlen);
    }
    else
    {
        printf("Couldn't get listening address of the socket.\n");
    }

    while (1)
    {
        // initialize parameters like you would with client.c
        pthread_t handlerThread;
        struct handlerParam clientParam;
        clientParam.caddr_len = sizeof(clientParam.caddr);
        clientParam.client_fd = accept(listen_fd, (struct sockaddr *)(&clientParam.caddr), &clientParam.caddr_len);
        if (clientParam.client_fd < 0)
        {
            if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
                continue;
            printf("Failure on accept:%s \n ", strerror(errno));
            break;
        }

        // now create it the thread and handle client request, terminates on user request
        pthread_create(&handlerThread, NULL, HandleClient, &clientParam);
    }

    // Close socket
    close(listen_fd);
    return EXIT_SUCCESS;
}

void Usage(char *progname)
{
    printf("usage: %s port \n", progname);
    exit(EXIT_FAILURE);
}

void PrintOut(int fd, struct sockaddr *addr, size_t addrlen)
{
    printf("Socket [%d] is bound to: \n", fd);
    if (addr->sa_family == AF_INET)
    {
        // Print out the IPV4 address and port

        char astring[INET_ADDRSTRLEN];
        struct sockaddr_in *in4 = (struct sockaddr_in *)(addr);
        inet_ntop(AF_INET, &(in4->sin_addr), astring, INET_ADDRSTRLEN);
        printf(" IPv4 address %s", astring);
        printf(" and port %d\n", ntohs(in4->sin_port));
    }
    else if (addr->sa_family == AF_INET6)
    {
        // Print out the IPV6 address and port

        char astring[INET6_ADDRSTRLEN];
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)(addr);
        inet_ntop(AF_INET6, &(in6->sin6_addr), astring, INET6_ADDRSTRLEN);
        printf(" IPv6 address %s", astring);
        printf(" and port %d\n", ntohs(in6->sin6_port));
    }
    else
    {
        printf(" [unknown address type]\n");
    }
}

void PrintReverseDNS(struct sockaddr *addr, size_t addrlen)
{
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int errcode = getnameinfo(addr, addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), 0);
    if (errcode)
    {
        printf(" (not found)");
    }
    else
    {
        printf(" DNS name: %s", hbuf);
    }
}

void PrintServerSide(int client_fd, int sock_family)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    if (getsockname(client_fd, (struct sockaddr *)&addr, &addrlen) == 0)
    {
        PrintOut(client_fd, (struct sockaddr *)&addr, addrlen);
    }
    else
    {
        printf("Couldn't get local address of the connected socket.\n");
    }
}

int Listen(char *portnum, int *sock_family)
{
    struct addrinfo hints, *res, *ressave;
    // Initialize addrinfo struct
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int n = getaddrinfo(NULL, portnum, &hints, &res);
    if (n < 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(n));
        return -1;
    }

    ressave = res;

    int listen_fd;
    do
    {
        // Create socket
        listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (listen_fd < 0)
        {
            continue;
        }

        // Allow reuse of the same address
        int on = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

        // Bind socket
        if (bind(listen_fd, res->ai_addr, res->ai_addrlen) == 0)
        {
            break;
        }

        close(listen_fd);
    } while ((res = res->ai_next) != NULL);

    // If no address succeeded, return failure
    if (res == NULL)
    {
        return -1;
    }

    // Listen on the socket
    if (listen(listen_fd, 10) < 0)
    {
        close(listen_fd);
        return -1;
    }

    // Save the socket family
    *sock_family = res->ai_family;

    // Free the addrinfo struct
    freeaddrinfo(ressave);

    return listen_fd;
}

void *HandleClient(void *arg)
{
    struct handlerParam *clientParam = (struct handlerParam *)arg;
    int client_fd = clientParam->client_fd;
    struct sockaddr *caddr = (struct sockaddr *)&clientParam->caddr;
    socklen_t caddr_len = clientParam->caddr_len;
    // Print the client-side address
    PrintOut(client_fd, caddr, caddr_len);

    // Print the server-side address
    PrintServerSide(client_fd, caddr->sa_family);

    // Print reverse DNS lookup
    PrintReverseDNS(caddr, caddr_len);

    // Echo the message back to the client
    char buf[1024];
    ssize_t bytesRead;
    while ((bytesRead = recv(client_fd, buf, sizeof(buf) - 1, 0)) > 0)
    {
        buf[bytesRead] = '\0';
        printf("Received from client: %s\n", buf);
        // Echo back the message to the client
        ssize_t bytesSent = send(client_fd, buf, bytesRead, 0);
        if (bytesSent < 0)
        {
            fprintf(stderr, "Error sending data back to the client: %s\n", strerror(errno));
            break;
        }
    }

    if (bytesRead < 0)
    {
        fprintf(stderr, "Error reading from client: %s\n", strerror(errno));
    }

    // Close the client connection and release resources
    close(client_fd);
    pthread_exit(NULL);
}