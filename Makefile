CFLAGS   = -g
CPPFLAGS = -pthread
LDFLAGS  = -pthread
LDLIBS   = -lncurses -lform

all: client server

client: client.o
	$(CC) $(LDFLAGS) -o client client.o $(LDLIBS)

client.o: client.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c client.c

server: server.o
	$(CC) $(LDFLAGS) -o server server.o

server.o: server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c server.c

clean:
	rm client server *.o
