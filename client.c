#include <stdio.h>
#include <ncurses.h>
#include <form.h>
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
#define MAXBUFSIZE 1024
#define MAXUSERNAMESIZE 25

int sock;

pthread_t in_thread;

WINDOW *chat_window_border, *chat_window, *input_window_border, *input_window;

FILE *log_file;

void exit_client()
{
	endwin();
	if (sock) {
		send(sock, "D", strlen("D"), 0);
		close(sock);
	}
	if (log_file)
		fclose(log_file);
	exit(0);
}

void interface_init()
{
	initscr();
	nocbreak();

	chat_window_border = newwin(LINES-6, 0, 0, 0);
	chat_window = derwin(chat_window_border, LINES-8, COLS-2, 1, 1);

	scrollok(chat_window, TRUE);

	input_window_border = newwin(5, 0, LINES-6, 0);
	input_window = derwin(input_window_border, 3, COLS-2, 1, 1);

	scrollok(input_window, TRUE);

	wborder(chat_window_border, 0, 0, 0, 0, 0, 0, 0, 0);
	wborder(input_window_border, 0, 0, 0, 0, 0, 0, 0, 0);

	wrefresh(chat_window_border);
	wrefresh(input_window_border);
}

void * input_thread()
{
	int ch;
	char *msg;
	char final_msg[MAXBUFSIZE] = "M|";

	FIELD *fields[2];
	fields[0] = new_field(3, COLS-2, LINES-5, 1, 0, 0);
	fields[1] = NULL;

	FORM *message_form = new_form(fields);
	post_form(message_form);

	field_opts_on(fields[0], O_VISIBLE);

	while (1) {
		ch = wgetch(input_window);
		switch (ch) {
			case '\n':
				form_driver(message_form, REQ_NEXT_FIELD);
				msg = field_buffer(fields[0], 0);
				form_driver(message_form, REQ_PREV_FIELD);
				form_driver(message_form, REQ_CLR_FIELD);

				for (int i = strlen(msg); i >= 0; i--)
					if (*(msg+i) == ' ' && (i == 0 || *(msg+i-1) == ' '))
						*(msg+i) = '\0';

				if (strlen(msg) == 0)		
					break;

				strncat(final_msg, msg, MAXBUFSIZE);
				send(sock, final_msg, strlen(final_msg), 0);
				final_msg[2] = '\0';

				werase(input_window);

				break;
			case 127:
				form_driver(message_form, REQ_DEL_PREV);
				break;
			case KEY_LEFT:
				form_driver(message_form, REQ_PREV_CHAR);
				break;
			case KEY_RIGHT:
				form_driver(message_form, REQ_NEXT_CHAR);
				break;
			default:
				form_driver(message_form, ch);
				break;
		}
	}

	return NULL;
}

int main (int argc, char *argv[])
{
	if (argc < 2) {
		char *help = "Usage: chatty <ip-address> [-u username]";
		printf("%s\n", help);
		exit(1);
	}

	signal(SIGINT, exit_client);

	log_file = fopen("client.log", "w");

	sock = socket(AF_INET, SOCK_STREAM, 0);

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

	for (int i = 2; i < argc; i++)
		if (!strcmp(argv[i], "-u")) {
			if (!argv[i+1]) {
				printf("Option %s requires an value.\n", argv[2]);
				close(sock);
				exit(1);
			}
			strcat(username, argv[(i++ + 1)]);
			send(sock, username, sizeof(username), 0);
		}

	interface_init();

	pthread_create(&in_thread, NULL, input_thread, NULL);

	while (1) {
		char msg[MAXBUFSIZE];
		long length = 0;

		while (1) {
			length += recv(sock, &msg+length, sizeof(msg)-length, 0);
			if (length <= 0) {
				wprintw(chat_window, "Lost connection to server.\n");
				goto disconnect;
			} else if (msg[length-1] == '\0')
				break;
		}
		// TODO: Log chat window in client.
		message_received:
		if (!strncmp(msg, "S|", 2)) {
			wprintw(chat_window, "%s\n", msg+2);
		}
		wrefresh(chat_window);
		wrefresh(input_window);

		for (int i = 0; i < length-1; i++)
			if (msg[i] == '\0') {
				memmove(msg, msg+i+1, strlen(msg+i+1)+1);
				length = strlen(msg+1);
				goto message_received;
			}
	}

	// TODO: Wait for client user to send SIGINT.
	disconnect:
	pthread_cancel(in_thread);

	werase(input_window);
	werase(input_window_border);

	wrefresh(input_window);
	wrefresh(input_window_border);

	delwin(input_window);
	delwin(input_window_border);

	sleep(2);
	exit_client();

	return 0;
}
