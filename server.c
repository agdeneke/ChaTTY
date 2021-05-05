#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
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
	struct sockaddr_in client_addr;
	char user_name[MAXUSERNAMESIZE];
	pthread_t cli_thread;
};

enum disc_reason {
	disc_leave,
	disc_crash,
	disc_kicked
};

pthread_attr_t attr;

struct client clients[MAXCLIENTS];

FILE *log_file;

void printf_log(const char *message)
{
	printf("%s\n", message);
	fwrite(message, sizeof(char), strlen(message), log_file);
	fwrite("\n", sizeof(char), 1, log_file);
	fflush(log_file);
}

void broadcast_message(char *msg)
{
	for (int i = 0; i < MAXCLIENTS; i++) {
		if (clients[i].user_sock != 0)
			send(clients[i].user_sock, msg, strlen(msg)+1, 0);
	}
}

int set_username(struct client *cli, char *user_name)
{
	if (strlen(user_name) > MAXUSERNAMESIZE)
		return 1;

	for (int i = 0; i < MAXCLIENTS; i++)
		if (clients[i].user_sock != 0 && !strcmp(clients[i].user_name, user_name))
			return 2;

	strcpy(cli->user_name, user_name);

	return 0;
}

void disconnect_client(struct client *cli, enum disc_reason reason)
{
	char disc_msg[MAXBUFSIZE] = "S|";

	close(cli->user_sock);

	strcat(disc_msg, cli->user_name);
	strcat(disc_msg, " has disconnected. (");

	if (reason == disc_leave)
		strcat(disc_msg, "Left");
	else if (reason == disc_crash)
		strcat(disc_msg, "Crashed");
	else if (reason == disc_kicked)
		strcat(disc_msg, "Kicked");

	strcat(disc_msg, ")");

	cli->user_sock = 0;

	printf_log(disc_msg+2);
	broadcast_message(disc_msg);

	num_of_clients--;
}

void exit_server()
{
	printf_log("\nShutting down server...");
	for (int i = 0; i < MAXCLIENTS; i++) {
		if (clients[i].user_sock != 0)
			disconnect_client(&clients[i], disc_kicked);
		pthread_cancel(clients[i].cli_thread);
	}

	if (sock)
		close(sock);
	if (log_file != NULL)
		fclose(log_file);
	pthread_attr_destroy(&attr);

	exit(0);
}

void * client_thread(void *cli)
{
	struct client *cl = cli;

	while (1) {
		char msg[MAXBUFSIZE] = "";
		long length = 0;

		while (1) {
			length += recv(cl->user_sock, &msg+length, sizeof(msg)-length, 0);
			if (length <= 0) {
				disconnect_client(cl, disc_crash);
				pthread_exit(NULL);
			} else if (msg[length-1] == '\0')
				break;
		}
		
		if (!strncmp(msg, "D", 1)) {
			disconnect_client(cl, disc_leave);
			pthread_exit(NULL);
		}
		if (!strncmp(msg, "M|", 2)) {
			if (!strncmp(msg+2, "/", 1)) {
				if (!strncmp(msg+3, "help", 4)) {
					char help_msg[MAXBUFSIZE] = "S|";
					strcat(help_msg, "To Be Implemented");
					send(cl->user_sock, help_msg, strlen(help_msg)+1, 0);
				} else if (!strncmp(msg+3, "list", 4)) {
					char list_msg[MAXBUFSIZE] = "S|";
					strcat(list_msg, "Users Online (");
					sprintf(list_msg+strlen(list_msg), "%i", num_of_clients);
					strcat(list_msg, "): ");
					int users_online = 0;
					for (int i = 0; i < MAXCLIENTS; i++) {
						if (clients[i].user_sock != 0) {
							if (users_online != 0)
								strcat(list_msg, ", ");
							strcat(list_msg, clients[i].user_name);
							users_online++;
						}
					}
					send(cl->user_sock, list_msg, strlen(list_msg)+1, 0);
				} else if (!strncmp(msg+3, "me", 2)) {
					if (strlen(msg) > 6 && *(msg+5) == ' ') {
						char me_msg[MAXBUFSIZE] = "S|";
						strcat(me_msg, cl->user_name);
						strcat(me_msg, " ");
						strcat(me_msg, msg+6);
						printf_log(me_msg+2);
						broadcast_message(me_msg);
					}
				}
			} else { 
				char user_msg[MAXBUFSIZE] = "S|";
				strcat(user_msg, "[");
				strcat(user_msg, cl->user_name);
				strcat(user_msg, "] ");
				strcat(user_msg, msg+2);
				printf_log(user_msg+2);
				broadcast_message(user_msg);
			}
		}
		if (!strncmp(msg, "U|", 2)) {
			char user_name[MAXUSERNAMESIZE];
			int ret = set_username(cli, msg+2);
			if (ret != 0) {
				char name_set_error_msg[MAXBUFSIZE] = "S|";
				if (ret == 1)
					strcat(name_set_error_msg, "Your selected username is too large.");
				else if (ret == 2)
					strcat(name_set_error_msg, "Your selected username is in use.");
				send(cl->user_sock, name_set_error_msg, strlen(name_set_error_msg)+1, 0);
			} else {
				strcpy(user_name, msg+2);
			}
		}

		for (int i = 0; i < length-1; i++)
			if (msg[i] == '\0') {
				memmove(msg, msg+i+1, strlen(msg+i+1)+1);
				length = strlen(msg);
			}
	}

	return NULL;
}

void add_client(int client_sock, struct sockaddr_in client_addr)
{
	static int anon_num;
	char connect_msg_ip[MAXBUFSIZE];
	char connect_msg[MAXBUFSIZE];
	char msg[MAXUSERNAMESIZE+2];
	char anon_user_name[MAXUSERNAMESIZE] = "Anonymous #";

	if (num_of_clients >= MAXCLIENTS) {
		char *full_msg = "S|Sorry, the server is full.";
		send(client_sock, full_msg, strlen(full_msg)+1, 0);
		close(client_sock);
		return;
	}

	clients[num_of_clients].user_sock = client_sock;
	clients[num_of_clients].client_addr = client_addr;
	pthread_create(&clients[num_of_clients].cli_thread,
				   &attr,
				   &client_thread,
				   &clients[num_of_clients]);

	snprintf(anon_user_name+strlen(anon_user_name),
				MAXUSERNAMESIZE-strlen(anon_user_name),
				"%i",
				(anon_num++ + 1));
	set_username(&clients[num_of_clients], anon_user_name);

	snprintf(connect_msg_ip, MAXBUFSIZE, "Client %s is connecting as %s", inet_ntoa(clients[num_of_clients].client_addr.sin_addr), anon_user_name);
	printf_log(connect_msg_ip);

	snprintf(connect_msg, MAXBUFSIZE, "S|%s (%i/%i) has connected.", anon_user_name, (num_of_clients++ + 1), MAXCLIENTS);
	printf_log(connect_msg+2);
	broadcast_message(connect_msg);
}

int main (int argc, char *argv[])
{
	char startup_msg[MAXBUFSIZE];
	struct sockaddr_in server;
	struct sockaddr_in client_addr;
	int client_sock;

	if (argc > 1)
		if (!strcmp(argv[1], "--help")) {
			char *help = "Usage: chatty-server [ip-address] [port] [maxclients]";
			printf("%s\n", help);
			exit(0);
		}

	// TODO: Log times.
	log_file = fopen("server.log", "w");

	printf_log("Initializing ChaTTY server...");

	signal(SIGINT, exit_server);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	sock = socket(AF_INET, SOCK_STREAM, 0);

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(PORT);

	if (bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr))) {
		snprintf(startup_msg, MAXBUFSIZE, "Failed to bind to IP address %s on port %i: %s", 
					inet_ntoa(server.sin_addr), ntohs(server.sin_port), strerror(errno));
		printf_log(startup_msg);
		exit(1);
	}

	snprintf(startup_msg, MAXBUFSIZE, "Bound to IP address %s on port %i",
				inet_ntoa(server.sin_addr), ntohs(server.sin_port));
	printf_log(startup_msg);

	listen(sock, 1);

	num_of_clients = 0;

	socklen_t client_addr_len = sizeof(client_addr);

	while (1) {
		client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
		add_client(client_sock, client_addr);
	}

	return 0;
}
