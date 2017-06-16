CFLAGS   = -g
CPPFLAGS = -pthread
LDFLAGS  = -pthread
LDLIBS   = -lncurses -lform

all: chatty-cli chatty-serv

chatty-cli: client.o
	$(CC) $(LDFLAGS) -o chatty-cli client.o $(LDLIBS)

client.o: client.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c client.c

chatty-serv: server.o
	$(CC) $(LDFLAGS) -o chatty-serv server.o

server.o: server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c server.c

clean:
	rm chatty-cli chatty-serv *.o
