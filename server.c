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
	char user_name[MAXUSERNAMESIZE];
	pthread_t cli_thread;
};

enum disc_reason {
	disc_leave,
	disc_crash,
	disc_server_shutdown,
	disc_other
};

pthread_attr_t attr;

struct client clients[MAXCLIENTS];

FILE *log_file;

void printf_log(const char *message)
{
	printf("%s\n", message);
	fwrite(message, sizeof(char), strlen(message), log_file);
	fwrite("\n", sizeof(char), 1, log_file);
}

void broadcast_message(char *msg)
{
	for (int i = 0; i < MAXCLIENTS; i++) {
		if (clients[i].user_sock != 0)
			send(clients[i].user_sock, msg, strlen(msg)+1, 0);
	}
}

void change_username(struct client *cli, char *new_username)
{
	char name_change_msg[MAXBUFSIZE] = "S|";
	
	if (strlen(new_username) > MAXUSERNAMESIZE) {
		strcat(name_change_msg, "Your username is too large.");
		send(cli->user_sock, name_change_msg, strlen(name_change_msg)+1, 0);
		return;
	}
	for (int i = 0; i < MAXCLIENTS; i++) {
		if (!strcmp(clients[i].user_name, new_username)) {
			strcat(name_change_msg, "Your username is already used.");
			send(cli->user_sock, name_change_msg, strlen(name_change_msg)+1, 0);
			return;
		}
	}

	strcat(name_change_msg, cli->user_name);
	strcat(name_change_msg, " has changed their name to ");
	strcpy(cli->user_name, new_username);
	strcat(name_change_msg, cli->user_name);
	printf_log(name_change_msg+2);
	broadcast_message(name_change_msg);
}

void disconnect_client(struct client *cli, enum disc_reason reason, ...)
{
	close(cli->user_sock);

	char disc_msg[MAXBUFSIZE] = "S|";
	strcat(disc_msg, cli->user_name);
	strcat(disc_msg, " has disconnected. (");

	if (reason == disc_leave)
		strcat(disc_msg, "Left");
	else if (reason == disc_crash)
		strcat(disc_msg, "Crashed");
	else if (reason == disc_server_shutdown)
		strcat(disc_msg, "Server shutting down");

	strcat(disc_msg, "}");

	cli->user_sock = 0;
	cli->user_name[0] = '\0';
	cli->cli_thread = 0;

	printf_log(disc_msg+2);
	broadcast_message(disc_msg);

	num_of_clients--;
}

void exit_server()
{
	printf_log("\nShutting down server...");
	for (int i = 0; i < MAXCLIENTS; i++) {
		if (clients[i].user_sock != 0)
			disconnect_client(&clients[i], disc_server_shutdown);
		pthread_cancel(clients[i].cli_thread);
	}

	if (sock)
		close(sock);
	if (log_file)
		fclose(log_file);
	pthread_attr_destroy(&attr);

	exit(0);
}

void * client_thread(void *cli)
{
	struct client *cl = cli;
	while (1) {
		char msg[MAXBUFSIZE] = "";
		if (recv(cl->user_sock, &msg, sizeof(msg), 0) <= 0) {
			disconnect_client(cl, disc_crash);
			pthread_exit(NULL);
		}
		if (!strncmp(msg, "D", 1)) {
			disconnect_client(cl, disc_leave);
			pthread_exit(NULL);
		}
		if (!strncmp(msg, "M|", 2)) {
			char user_msg[MAXBUFSIZE] = "S|";
			strcat(user_msg, "[");
			strcat(user_msg, cl->user_name);
			strcat(user_msg, "] ");
			strcat(user_msg, msg+2);
			printf_log(user_msg+2);
			broadcast_message(user_msg);
		}
		if (!strncmp(msg, "U|", 2)) {
			change_username(cl, msg+2);
		}
	}
	return NULL;
}

void add_client(int client_sock)
{
	static int anon_num;
	char connect_msg[MAXBUFSIZE] = "S|";

	if (num_of_clients >= MAXCLIENTS) {
		char *full_msg = "S|Sorry, the server is full.";
		send(client_sock, full_msg, strlen(full_msg)+1, 0);
		close(client_sock);
		return;
	}

	char anon_username[MAXUSERNAMESIZE] = "Anonymous #";
	snprintf(anon_username+strlen(anon_username),
				MAXUSERNAMESIZE-strlen(anon_username),
				"%i",
				(anon_num++ + 1));

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	clients[num_of_clients].user_sock = client_sock;
	pthread_create(&clients[num_of_clients].cli_thread,
				   &attr,
				   &client_thread,
				   &clients[num_of_clients]);
	strcpy(clients[num_of_clients].user_name, anon_username);

	snprintf(connect_msg+2, MAXBUFSIZE-2, "%s (%i/%i) has connected.", anon_username, (num_of_clients++ + 1), MAXCLIENTS);

	printf_log(connect_msg+2);
	broadcast_message(connect_msg);
}

int main (int argc, char *argv[])
{
	char message[MAXBUFSIZE];

	if (argc > 1)
		if (!strcmp(argv[1], "--help")) {
			char *help = "Usage: chatty-server [ip-address] [port] [maxclients]";
			printf("%s\n", help);
			exit(0);
		}

	log_file = fopen("server.log", "w");

	printf_log("Initializing ChaTTY server...");

	signal(SIGINT, exit_server);

	sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(PORT);

	if (bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr))) {
		snprintf(message, MAXBUFSIZE, "Failed to bind to IP address %s on port %i: %s", 
				inet_ntoa(server.sin_addr), ntohs(server.sin_port), strerror(errno));
		printf_log(message);
		exit(1);
	}

	snprintf(message, MAXBUFSIZE, "Bound to IP address %s on port %i",
			inet_ntoa(server.sin_addr), ntohs(server.sin_port));
	printf_log(message);
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
