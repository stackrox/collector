#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>

#include <sys/time.h>

ssize_t timed_read(int fd, void* buf, size_t count, std::uint64_t& nanos) {
  timespec start;
  timespec end;
  clock_gettime(CLOCK_REALTIME, &start);

  ssize_t ret = read(fd, buf, count);

  clock_gettime(CLOCK_REALTIME, &end);

  nanos = end.tv_nsec - start.tv_nsec;
  return ret;
}

int timed_close(int fd, std::uint64_t& nanos) {
  timespec start;
  timespec end;
  clock_gettime(CLOCK_REALTIME, &start);

  int ret = close(fd);

  clock_gettime(CLOCK_REALTIME, &end);

  nanos = end.tv_nsec - start.tv_nsec;
  return ret;
}

int main() {
  char buf[10] = {0};

  while (1) {
    int devnull = open("/dev/null", O_RDONLY);
    if (devnull < 0) {
      perror("open");
      return 1;
    }

    std::uint64_t nanos;
    timed_read(devnull, buf, 10, nanos);
    std::cout << "r " << nanos << std::endl;
    timed_close(devnull, nanos);
    std::cout << "c " << nanos << std::endl;
  }

  return 0;
}
