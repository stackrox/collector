
#include <cstdio>
#include <cstdlib>

#include <netinet/in.h>

bool startListeningPort(uint16_t port) {
  struct sockaddr_in sock_addr = {};
  int listener = socket(AF_INET, SOCK_STREAM, 0);

  if (listener < 0) {
    return false;
  }

  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_addr.sin_port = htons(port);

  if (bind(listener, (struct sockaddr*)(&sock_addr), sizeof(sock_addr)) < 0) {
    return false;
  }

  if (listen(listener, 1) < 0) {
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  if (!startListeningPort(1337)) {
    return -1;
  }
  return 0;
}
