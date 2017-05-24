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

WINDOW *chat_window;
WINDOW *input_window;

void exit_client()
{
	endwin();
	close(sock);
	exit(0);
}

void * input_thread()
{
/* TODO: The form corrupts the screen. */ 
	int ch;
	char *msg;
	char final_msg[MAXBUFSIZE] = "M|";

	FIELD *fields[2];
	fields[0] = new_field(3, COLS-2, LINES-5, 1, 0, 0);
	fields[1] = NULL;

	FORM *message_form = new_form(fields);
	post_form(message_form);

	wrefresh(chat_window);
	wrefresh(input_window);

	while (1) {
		ch = getch();
		switch (ch) {
			case '\n':
				form_driver(message_form, REQ_NEXT_FIELD);
				msg = field_buffer(fields[0], 0);
				form_driver(message_form, REQ_PREV_FIELD);
				form_driver(message_form, REQ_CLR_FIELD);

				for (int i = strlen(msg); i > 0; i--)
					if (*(msg+i) == ' ' && *(msg+i-1) == ' ')
						*(msg+i) = '\0';
				strncat(final_msg, msg, MAXBUFSIZE);
				send(sock, final_msg, strlen(final_msg), 0);
				break;
			default:
				form_driver(message_form, ch);
				wrefresh(chat_window);
				wrefresh(input_window);
				break;
		}
	}

	return NULL;
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
	chat_window = newwin(LINES-6, 0, 0, 0);
	input_window = newwin(5, 0, LINES-6, 0);

	wborder(chat_window, 0, 0, 0, 0, 0, 0, 0, 0);
	wborder(input_window, 0, 0, 0, 0, 0, 0, 0, 0);

	wmove(input_window, 1, 1);

	wrefresh(chat_window);
	wrefresh(input_window);

	in_thread = pthread_create(&in_thread, NULL, input_thread, NULL);

	while (1) {
		char msg[MAXBUFSIZE];
		if (recv(sock, &msg, sizeof(msg), 0) <= 0) {
			wprintw(chat_window, "Lost connection to server.\n");
			break;
		}
		wmove(chat_window, 1, 1);
		if (!strncmp(msg, "S|", 2)) {
			wprintw(chat_window, "%s\n", msg+2);
		}
		if (!strncmp(msg, "M|", 2)) {
			wprintw(chat_window, "%s\n", msg+2);
		}
		wmove(input_window, 1, 1);
		wrefresh(chat_window);
		wrefresh(input_window);
	}

	/* TODO: Wait for user input. */
	sleep(2);
	exit_client();

	return 0;
}
