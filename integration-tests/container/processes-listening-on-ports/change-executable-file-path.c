#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char* argv[]) {
  int sockfd;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Bind the socket to a port
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8082);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    perror("Binding failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(sockfd, 5) == -1) {
    perror("Listening failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Listening on port 8082...\n");

  sleep(10);

  // Change the process name
  argv[0][0] = 'a';

  printf("Process name changed to: %s\n", argv[0]);

  sleep(10000);

  return 0;
}
