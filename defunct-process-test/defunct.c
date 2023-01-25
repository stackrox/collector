#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
	int pid;
	char c;
	
	pid = fork();

	if (pid) {
		printf("Zombie PID: %d\n", pid);
	} else {
		exit(0);
	}

	read(0, &c, 1);

	wait(NULL);

	return 0;
}
