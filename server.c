#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>

#define PORT 6666
#define MAXCLIENTS 4
#define MAXBUFSIZE 1024
#define MAXUSERNAMESIZE 25

int sock;
int num_of_clients;

struct client {
	int user_sock;
	char *user_name;
	pthread_t cli_thread;
};

pthread_attr_t attr;

struct client clients[MAXCLIENTS];

void broadcast_message(char *msg, ...)
{
	va_list al;

	va_start(al, msg);
	for (int i = 0; i < MAXCLIENTS; i++)
		if (clients[i].user_sock != 0)
			send(clients[i].user_sock, msg, sizeof(msg), 0);
	va_end(al);
}

void disconnect_client(struct client cli)
{
	close(cli.user_sock);

	char disc_msg[MAXBUFSIZE] = "S|";
	strcat(disc_msg, cli.user_name);
	strcat(disc_msg, " has disconnected.");

	cli.user_sock = 0;
	cli.user_name = NULL;
	cli.cli_thread = 0;

	printf("%s\n", disc_msg+2);
	broadcast_message(disc_msg);

	num_of_clients--;
}

void exit_server()
{
	printf("\nShutting down server...\n");
	for (int i = 0; i < MAXCLIENTS; i++) {
		if (clients[i].user_sock != 0)
			disconnect_client(clients[i]);
		pthread_cancel(clients[i].cli_thread);
	}
	if (sock)
		close(sock);
	pthread_attr_destroy(&attr);
	exit(0);
}

void * client_thread(void *cli)
{
	struct client *cl = cli;
	while (1) {
		char msg[MAXBUFSIZE];
		if (recv(cl->user_sock, &msg, sizeof(msg), 0) <= 0) {
			disconnect_client(*cl);
			pthread_exit(NULL);
		}
		if (!strncmp(msg, "M|", 2)) {
			broadcast_message(msg, cl->user_name);
		}
		if (!strncmp(msg, "U|", 2)) {
			char name_change_msg[MAXBUFSIZE] = "S|";
			strcat(name_change_msg, cl->user_name);
			strcat(name_change_msg, " has changed their name to ");
			cl->user_name = msg+2;
			strcat(name_change_msg, cl->user_name);
			printf("%s\n", name_change_msg+2);
			broadcast_message(name_change_msg);
		}
	}
	return NULL;
}

void add_client(int client_sock)
{
	static int anon_num;
	if (num_of_clients >= MAXCLIENTS) {
		char *full_msg = "S|Sorry, the server is full.";
		send(client_sock, full_msg, sizeof(full_msg), 0);
		close(client_sock);
		return;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
	clients[num_of_clients].user_sock = client_sock;
	clients[num_of_clients].cli_thread = pthread_create(&clients[num_of_clients].cli_thread,
														&attr,
														&client_thread,
														&clients[num_of_clients]);
	char anon_username[MAXUSERNAMESIZE] = "Anonymous #";
	snprintf(anon_username+strlen(anon_username),
				MAXUSERNAMESIZE-strlen(anon_username),
				"%i",
				(anon_num++ + 1));
	clients[num_of_clients].user_name = anon_username;
	printf("%s (%i/%i) has connected.\n", anon_username, (num_of_clients++ + 1), MAXCLIENTS);
}

int main (int argc, char *argv[])
{
	if (argc > 1)
		if (!strcmp(argv[1], "--help")) {
			char *help = "Usage: chatty-server [ip-address] [port] [maxclients]";
			printf("%s\n", help);
			exit(0);
		}

	printf("Initializing ChaTTY server...\n");

	sock = socket(AF_INET, SOCK_STREAM, 0);

	signal(SIGINT, exit_server);

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(PORT);

	if (bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr))) {
		printf("Failed to bind to IP address %s on port %i: %s\n", 
				inet_ntoa(server.sin_addr), ntohs(server.sin_port), strerror(errno));
		exit(1);
	}

	printf("Bound to IP address %s on port %i\n",
			inet_ntoa(server.sin_addr), ntohs(server.sin_port));
	listen(sock, 1);

	num_of_clients = 0;

	int client_sock;

	while (1) {
		client_sock = accept(sock, NULL, NULL);
		add_client(client_sock);
	}

	close(sock);
	return 0;
}
