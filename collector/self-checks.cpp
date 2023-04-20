
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/errno.h>
#include <sys/select.h>
#include <sys/wait.h>

const int g_connection_retries = 5;
const int g_timeout_seconds = 10;
const int g_sleep_seconds = 1;

/**
 * @brief starts a listening socket for the given port, and waits for a connection
 *        for a short time. If nothing connects in time, the socket is closed.
 */
bool startListeningPort(uint16_t port) {
  bool result = false;
  struct sockaddr_in sock_addr = {};
  int connection = -1;
  fd_set sockets;
  struct timeval timeout = {.tv_sec = g_timeout_seconds};

  int listener = socket(AF_INET, SOCK_STREAM, 0);

  if (listener < 0) {
    return false;
  }

  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_addr.sin_port = htons(port);

  if (bind(listener, (struct sockaddr*)(&sock_addr), sizeof(sock_addr)) < 0) {
    goto err;
  }

  if (listen(listener, 1) < 0) {
    goto err;
  }

  FD_ZERO(&sockets);
  FD_SET(listener, &sockets);

  if (select(1, &sockets, NULL, NULL, &timeout) <= 0) {
    // timeout or error
    goto err;
  }

  // otherwise listener is readable, and therefore
  // someone is trying to connect.
  if ((connection = accept(listener, NULL, NULL)) < 0) {
    goto err;
  }

  result = true;
err:
  close(connection);
  close(listener);
  return result;
}

/**
 * @brief Connects to the given port, on the loopback interface.
 * Attempts a number of times, and then returns false if it is still unable to connect.
 */
bool connectToPort(uint16_t port) {
  struct sockaddr_in server;
  int connection = socket(AF_INET, SOCK_STREAM, 0);
  bool result = false;

  if (connection < 0) {
    goto err;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  server.sin_port = htons(port);

  for (int i = 0; i < g_connection_retries; i++) {
    if (connect(connection, (struct sockaddr*)(&server), sizeof(server)) < 0) {
      if (errno == ECONNREFUSED) {
        continue;
      }
      goto err;
    }

    result = true;
    sleep(g_sleep_seconds);
  }
err:
  close(connection);
  return result;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    // expect port number as argument
    return EINVAL;
  }

  int port = std::stoi(argv[1]);
  pid_t child = fork();
  int status = -1;

  if (child == 0) {
    if (!startListeningPort(port)) {
      return errno;
    }
    return 0;
  } else if (child == -1) {
    // failed to fork for whatever reason
    return errno;
  } else {
    if (!connectToPort(port)) {
      return errno;
    }

    waitpid(child, &status, 0);
  }
  return status;
}
