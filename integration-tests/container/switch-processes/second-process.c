#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char* argv[]) {
  int sockfd;

  // Check if sockfd is provided as a command line argument
  if (argc == 2) {
    sockfd = atoi(argv[1]);
  } else {
    printf("Usage: %s sockfd\n", argv[0]);
  }

  // Listen for incoming connections
  if (listen(sockfd, 5) == -1) {
    perror("Listening failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Listening in the child process\n");

  sleep(10000);

  return 0;
}
