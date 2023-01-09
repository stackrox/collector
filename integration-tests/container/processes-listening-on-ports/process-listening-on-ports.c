#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

int openPort(int port) {
	int server_fd;
	struct sockaddr_in address;
	int opt = 1;

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(server_fd, SOL_SOCKET,
				SO_REUSEADDR | SO_REUSEPORT, &opt,
				sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr*)&address,
			sizeof(address))
		< 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	return server_fd;
}

void closePort(int server_fd) {
	close(server_fd);
}

int getActionFromFile(char actionFile[], char action[]) {
	FILE* ptr;
	int port;

	ptr = fopen(actionFile, "r");

	while (ptr == NULL) {
		sleep(0.1);
		ptr = fopen(actionFile, "r");
	}

	fscanf(ptr, "%s %d", action, &port);

	fclose(ptr);
	remove(actionFile);

	return port;
}

int main(int argc, char const* argv[])
{
	int port;
	int server_fd[65535];
	char *action = malloc(16 * sizeof(char));
	char actionFile[200];


	if (argc == 2) {
		strcpy(actionFile, argv[1]);
	} else {
		strcpy(actionFile, "/tmp/action_file.txt");
	}

	for (int i = 0; i < 65535; i++) {
		server_fd[i] = -1;
	}

	while (1) {
		port = getActionFromFile(actionFile, action);
		printf("action= %s port= %i\n", action, port);
		if (strcmp(action, "open") == 0) {
			if (server_fd[port] == -1) {
				server_fd[port] = openPort(port);
			} else {
				closePort(server_fd[port]);
				server_fd[port] = -1;
			}
		} else if (strcmp(action, "close") == 0) {
			closePort(server_fd[port]);
			server_fd[port] = -1;
		} else {
			printf("Unknown action: %s\n", action);
		}
	}

	return 0;
}
