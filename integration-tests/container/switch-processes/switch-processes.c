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

  // Create a child process
  pid_t child_pid = fork();
  if (child_pid == -1) {
    perror("Fork failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  if (child_pid == 0) {
    // This is the child process

    if (setsid() == -1) {
      perror("setsid failed");
      exit(EXIT_FAILURE);
    }

    // Pass the socket file descriptor to the child
    char sockfd_str[32];
    snprintf(sockfd_str, sizeof(sockfd_str), "%d", sockfd);
    char* const child_args[] = {"second-process", sockfd_str, NULL};

    // Replace the child process with a new program
    if (execve("./second-process", child_args, NULL) == -1) {
      perror("Execve failed");
      close(sockfd);
      exit(EXIT_FAILURE);
    }
  } else {
    sleep(10);
    exit(EXIT_SUCCESS);
  }

  sleep(10000);

  return 0;
}
