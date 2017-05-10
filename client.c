#include <stdio.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>

#define PORT 6666
#define MAXBUFSIZE 1024
#define MAXUSERNAMESIZE 25

int sock;

void exit_client()
{
	printf("Exiting ChaTTY...\n");
	close(sock);
	exit(0);
}

int main (int argc, char *argv[])
{
	if (argc < 3) {
		char *help = "Usage: chatty <ip-address> <username>";
		printf("%s\n", help);
		exit(1);
	}
	
	printf("ChaTTY\n");
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	
	signal(SIGINT, exit_client);
	
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(argv[1]);
	server.sin_port = htons(PORT);
	
	if (connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr))) {
		printf("Error: Couldn't connect to %s: %s\n", argv[1], strerror(errno));
		close(sock);
		exit(1);
	}
	
	char username[MAXUSERNAMESIZE+2] = "U|";
	strcat(username, argv[2]);
	
	send(sock, username, sizeof(username), 0);
	
	initscr();
	
	while (1) {
		
	}
	
	return 0;
}
