#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Spawn a thread, then exec an arbitrary binary from it. Depending on glibc
 * version, this will produce clone + exec or clone3 + exec. Exec from a thread
 * will tear down any other threads and reassign the thread group leader pid to
 * this thread (it's nicely explained in "map ptrace", section "execve(2) under
 * ptrace". This should cause threads cleanup logic in Falco, and produce
 * visible effect: the recorder process should have "thread_exec" file path,
 * rather than ls (coreutils).
 */

void* threadTest(void* vargp) {
  sleep(5);
  char* argument_list[] = {"/bin/ls", NULL};
  execvp(*argument_list, argument_list);
  return NULL;
}

int main() {
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, threadTest, NULL);
  while (1) {
  };
  exit(0);
}
