#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int array[200];
int n_conn = 0;

void sig_handler(int signum) {
	for(int i = 0; i < n_conn; ++i) {
		kill(array[i], SIGKILL);
	}

	exit(0);
}
	


int main(int argc, char **argv) {

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		exit(1);
	}	

	if (argc != 2) {
		printf("Usage: ./perf n_clients\n");
		printf("n_client: Number of clients\n");
		printf("Ex: ./perf 40\n");
		exit(1);
	}

	n_conn = atoi(argv[1]);
	memset(array, 0, sizeof array);
	int s_port = 4010;
	
	for(int i = 0; i < n_conn; ++i) {
		int temp = s_port + i;
		int pid = fork();
		if (!pid) {
			char port[5];

			snprintf(port, sizeof port, "%d", temp);
			s_port++;
			execl("./client", "./client", "2", port, "3233", NULL);
		} else {
			array[i] = pid;
		}
		sleep(1);
	}
	sleep(1110);

	return 0;
}			

