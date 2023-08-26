#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "msg.h"
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <inttypes.h>

#define BUF 256

void Usage(char *progname);

// from client.c
int LookupName(char *name,
               unsigned short port,
               struct sockaddr_storage *ret_addr,
               size_t *ret_addrlen);

int Connect(const struct sockaddr_storage *addr,
            const size_t addrlen,
            int *ret_fd);

void put(int socket_fd);
void get(int socket_fd);

int main(int argc, char **argv)
{

    // Check if the given args for dbclient.c is correct
    if (argc != 3)
    {
        Usage(argv[0]);
    }

    // Check if given port is valid
    unsigned short port = 0;
    if (sscanf(argv[2], "%hu", &port) != 1)
    {
        Usage(argv[0]);
    }

    // Get an appropriate sockaddr structure.
    struct sockaddr_storage addr;
    size_t addrlen;
    if (!LookupName(argv[1], port, &addr, &addrlen))
    {
        Usage(argv[0]);
    }

    // Connect to the remote host.
    int socket_fd;
    if (!Connect(&addr, addrlen, &socket_fd))
    {
        Usage(argv[0]);
    }

    // Modified code pulled from a5 (db.c, dbWrapper.c)
    // Reads from and write to server depending on choice
    int8_t choice, flag;
    flag = 1;
    while (flag)
    {
        printf("Enter your choice (1 to put, 2 to get, 0 to quit): ");
        scanf("%" SCNd8 "%*c", &choice);

        switch (choice)
        {
        case 1:
            put(socket_fd);
            break;
        case 2:
            get(socket_fd);
            break;
        default:
            flag = 0;
        }
    }

    // Clean up.
    close(socket_fd);
    return EXIT_SUCCESS;
}

void Usage(char *progname)
{
    printf("usage: %s  hostname port \n", progname);
    exit(EXIT_FAILURE);
}

int LookupName(char *name,
               unsigned short port,
               struct sockaddr_storage *ret_addr,
               size_t *ret_addrlen)
{
    struct addrinfo hints, *results;
    int retval;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Do the lookup by invoking getaddrinfo().
    if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0)
    {
        printf("getaddrinfo failed: %s", gai_strerror(retval));
        return 0;
    }

    // Set the port in the first result.
    if (results->ai_family == AF_INET)
    {
        struct sockaddr_in *v4addr =
            (struct sockaddr_in *)(results->ai_addr);
        v4addr->sin_port = htons(port);
    }
    else if (results->ai_family == AF_INET6)
    {
        struct sockaddr_in6 *v6addr =
            (struct sockaddr_in6 *)(results->ai_addr);
        v6addr->sin6_port = htons(port);
    }
    else
    {
        printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
        freeaddrinfo(results);
        return 0;
    }

    // Return the first result.
    assert(results != NULL);
    memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
    *ret_addrlen = results->ai_addrlen;

    // Clean up.
    freeaddrinfo(results);
    return 1;
}

int Connect(const struct sockaddr_storage *addr,
            const size_t addrlen,
            int *ret_fd)
{
    // Create the socket.
    int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        printf("socket() failed: %s", strerror(errno));
        return 0;
    }

    // Connect the socket to the remote host.
    int res = connect(socket_fd,
                      (const struct sockaddr *)(addr),
                      addrlen);
    if (res == -1)
    {
        printf("connect() failed: %s", strerror(errno));
        return 0;
    }

    *ret_fd = socket_fd;
    return 1;
}

// Modified a5 code
void put(int socket_fd)
{
    // create msg struct to interact with server
    struct msg m;

    // initialize msg type to PUT
    m.type = PUT;

    printf("Enter the student name: ");

    // read the name from stdin
    // store it in m.rd.name
    // use fgets()
    // fgets doesnt remove newline. replace '\n' with '\0' in r.name.
    // check if given name is a null character or not before moving forward
    m.rd.name[0] = '\0';
    while (m.rd.name[0] == '\0')
    {
        fgets(m.rd.name, MAX_NAME_LENGTH, stdin);
        m.rd.name[strlen(m.rd.name) - 1] = '\0';
        if (m.rd.name[0] == '\0')
        {
            printf("Given name is an empty string. Please try again. \n");
        }
    }

    printf("Enter the record id: ");
    // read record id from stdin
    // store it in r.id
    scanf("%d", &m.rd.id);

    // write given name and record id to server
    write(socket_fd, &m, sizeof(m.rd));
}

// read the student record stored at position index in fd
void get(int socket_fd)
{
    // create msg struct to interact with server
    struct msg m;

    // initialize msg type to GET
    m.type = GET;

    printf("Enter the record id: ");
    //
    // WRITE THE CODE to get record index from stdin and store in it index
    scanf("%d", &m.rd.id);

    // read record s from fd
    // If the record has not been put already, print appropriate message
    // and return
    read(socket_fd, &m, sizeof(m.rd));
    if (strcmp(m.rd.name, "\0") == 0)
    {
        perror("Record does not exist, please enter it in.");
        return;
    };

    printf("Student name %s \n", m.rd.name);
    printf("Record id: %d \n", m.rd.id);
}
