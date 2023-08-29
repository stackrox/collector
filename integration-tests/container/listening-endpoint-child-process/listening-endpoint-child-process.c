#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8082

int main() {
  int server_socket, client_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  // Create socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == -1) {
    perror("Error creating socket");
    return EXIT_FAILURE;
  }

  // Set up server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind socket to the server address
  if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    perror("Error binding");
    close(server_socket);
    return EXIT_FAILURE;
  }

  // Listen for incoming connections
  if (listen(server_socket, 5) == -1) {
    perror("Error listening");
    close(server_socket);
    return EXIT_FAILURE;
  }

  pid_t child_pid = fork();

  if (child_pid == -1) {
    perror("Error forking");
  } else if (child_pid == 0) {
    sprintf("%s", "Hello\n");
    sleep(10000);
    exit(EXIT_SUCCESS);
  } else {
    sleep(10000);
  }

  // Close the server socket
  close(server_socket);

  return EXIT_SUCCESS;
}
