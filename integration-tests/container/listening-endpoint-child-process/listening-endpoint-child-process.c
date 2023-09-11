#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>  // For TCP socket options
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

  int flags = fcntl(server_socket, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }
  if (fcntl(server_socket, F_SETFL, flags) == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }

  // Set up server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // Enable SO_REUSEADDR option to allow multiple sockets to bind to the same port
  int reuseaddr = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1) {
    perror("Error setting SO_REUSEADDR");
    close(server_socket);
    return EXIT_FAILURE;
  }

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
  printf("%i\n", child_pid);

  if (child_pid == -1) {
    perror("Error forking");
  } else if (child_pid == 0) {
    // Child process
    printf("%s", "Hello (Child)\n");

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
      perror("Error listening");
      close(server_socket);
      return EXIT_FAILURE;
    }

    //// Accept connections in the child process
    // client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    // if (client_socket == -1) {
    //   perror("Error accepting connection in child process");
    //   close(server_socket);
    //   return EXIT_FAILURE;
    // }

    // Handle the connection in the child process as needed
    sleep(10);

    // close(client_socket);  // Close the client socket when done
    // close(server_socket);  // Close the server socket in the child process
  } else {
    printf("%s", "Bye (Parent)\n");

    //// Accept connections in the parent process
    // client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    // if (client_socket == -1) {
    //   perror("Error accepting connection in parent process");
    //   close(server_socket);
    //   return EXIT_FAILURE;
    // }
    //
    //// Handle the connection in the parent process as needed
    //
    // close(client_socket);  // Close the client socket when done
    // close(server_socket);  // Close the server socket in the parent process
    sleep(10);
    exit(EXIT_SUCCESS);
  }

  pid_t child_pid2 = fork();
  printf("%i\n", child_pid2);

  if (child_pid2 == -1) {
    perror("Error forking");
  } else if (child_pid2 == 0) {
    // Child process
    printf("%s", "Hello (Child)\n");

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
      perror("Error listening");
      close(server_socket);
      return EXIT_FAILURE;
    }

    //// Accept connections in the child process
    // client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    // if (client_socket == -1) {
    //   perror("Error accepting connection in child process");
    //   close(server_socket);
    //   return EXIT_FAILURE;
    // }

    // Handle the connection in the child process as needed
    sleep(10);

    // close(client_socket);  // Close the client socket when done
    // close(server_socket);  // Close the server socket in the child process
  } else {
    printf("%s", "Bye (Parent)\n");

    //// Accept connections in the parent process
    // client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    // if (client_socket == -1) {
    //   perror("Error accepting connection in parent process");
    //   close(server_socket);
    //   return EXIT_FAILURE;
    // }
    //
    //// Handle the connection in the parent process as needed
    //
    // close(client_socket);  // Close the client socket when done
    // close(server_socket);  // Close the server socket in the parent process
    sleep(10);
    exit(EXIT_SUCCESS);
  }

  return EXIT_SUCCESS;
}
