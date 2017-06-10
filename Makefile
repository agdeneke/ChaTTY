CFLAGS  = -g
LDFLAGS = -pthread
LDLIBS  = -lncurses -lform

all: client server

client: client.o
	$(CC) -o client client.o $(LDLIBS)

client.o: client.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c client.c

server: server.o
	$(CC) -o server server.o

server.o: server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c server.c

clean:
	rm client server *.o