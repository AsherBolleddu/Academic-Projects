CC = gcc -I .
CFLAGS = -Wall -Werror -std=gnu99 -pthread

all: dbserver dbclient

dbserver: dbserver.c
	$(CC) dbserver.c -o dbserver $(CFLAGS)

dbclient: dbclient.c
	$(CC) dbclient.c -o dbclient $(CFLAGS)

clean:
	rm -f dbserver dbclient
