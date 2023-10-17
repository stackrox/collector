#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>

static long
perf_event_open(struct perf_event_attr* hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags) {
  int ret;

  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                group_fd, flags);
  return ret;
}

int main(int argc, char** argv) {
  struct perf_event_attr pe;
  long long count;
  int fd;

  // How long to wait for events, in seconds
  int wait_interval = 10;

  // /sys/kernel/debug/tracing/events/sched/sched_process_exit/id
  int tracepoint_code = 310;

  if (argc == 3) {
    wait_interval = atoi(argv[1]);
    tracepoint_code = atoi(argv[2]);
  }

  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = PERF_TYPE_TRACEPOINT;
  pe.size = sizeof(struct perf_event_attr);
  // /sys/kernel/debug/tracing/events/sched/sched_process_exit/id
  pe.config = tracepoint_code;
  pe.disabled = 1;
  pe.sample_period = 1;
  pe.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_RAW;
  pe.comm = 1;

  fd = perf_event_open(&pe, -1, 0, -1, PERF_FLAG_FD_CLOEXEC);
  if (fd == -1) {
    fprintf(stderr, "Error opening leader %llx\n", pe.config);
    exit(EXIT_FAILURE);
  }

  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

  sleep(wait_interval);
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
  read(fd, &count, sizeof(long long));

  printf("%lld\n", count);

  close(fd);
}
