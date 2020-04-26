#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pulse/simple.h>
#include <pulse/error.h>

// #define SPORT "3233"
char SPORT[4];  //the port clients will be connecting to
#define BACKLOG 5 // how many pending client connections queue will hold
#define MAXLEN 1024

// #define CPORT "8001" // Port where client receives messages

char CPORT[100];
// default ID is 0
char ID[4];

pa_simple *sw = NULL, *sr = NULL;

void *thread_read(void *param);
void *thread_write(void *param);
void *thread_routine(void *param);
void send_message(int sockfd, char *msg, int msg_len);

int main(int argc, char **argv) {

	if (argc != 4) {
		printf("Usage: ./client GROUP_ID CLIENT_PORT_NO SERVER_PORT_NO\n");
		printf("GROUPID: Group to which this client belongs\n");
		printf("CLIENT_PORT_NO: Port at which client receives messages\n");
		printf("SERVER_PORT_NO: Port at which server receives messages (4 digits)\n");
		printf("Ex: ./client 2 4001 3233\n");
		exit(1);
	}

	/* The sample type to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 2
	};

	int error;
	/* Create the recording stream */
	if (!(sr = pa_simple_new(NULL, "record_client", PA_STREAM_RECORD, NULL, "record_client_stream", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish_record;
	}

	/* Create a new playback stream */
	if (!(sw = pa_simple_new(NULL, "playback_client", PA_STREAM_PLAYBACK, NULL, "playback_client_stream", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish_playback;
	}	

	memset(ID, 0, 4);
	memset(CPORT, 0, 100);
	memset(SPORT, 0, 4);
	strcpy(ID, argv[1]);
	strcpy(CPORT, argv[2]);
	strncpy(SPORT, argv[3], 4);

	argv[1][2] = '\0';

	printf("ID is %s\n", ID);
	printf("CPORT is %s\n", CPORT);

	pthread_t thread_r, thread_w;

	if(pthread_create(&thread_r, NULL, thread_read, NULL)) {
		perror("pthread_create: thread_read");
		exit(1);
	}

	if(pthread_create(&thread_w, NULL, thread_write, NULL)) {
		perror("pthread_create: thread_write");
		exit(1);
	}

	// wait for 1st thread to finish
	if(pthread_join(thread_r, NULL)) {
		perror("pthread_join: thread_r");
		exit(1);
	}

	if(pthread_join(thread_w, NULL)) {
		perror("pthread_join: thread_w");
		exit(1);
	}
	return 0;
	
finish_record:
	if (sr)
		pa_simple_free(sr);
	return -1;

finish_playback:
	if (sw)
		pa_simple_free(sw);
	return -1;
}

/*
 * Protocol: When client connects to server. It does
 * 1. Send it's group id
 * 2. Send it's receiving PORT to server
 * 3. Enter while loop
 * 4. Take input from user and send to server
 * 5. Terminate when message sending fails (or) server closes
 */

void *thread_write(void *param) {
	int error, sockfd;
	char msg[MAXLEN];
	struct addrinfo hints, *res;

	// Load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use IPV4 or IPV6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me

	int ginfo = getaddrinfo(NULL, SPORT, &hints, &res);

	if(ginfo != 0) {
		// TODO: Make error statement more clear by including
		// the type of error. Check getaddrinfo man. Use errno
		perror("getaddrinfo");
		exit(1);
	}


	// make a socket, bind it, and listen on it:
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1) {
		perror("socket");
		exit(1);
	}

	// Connect
	while(connect(sockfd, res->ai_addr, res->ai_addrlen) != -1) {
		perror("connect");
	};

	// Send GroupID first
	memset(msg, 0, MAXLEN);
	strcpy(msg, ID);
	send_message(sockfd, msg, 1);
	memset(msg, 0, MAXLEN);
	strncpy(msg, CPORT, 4);
	send_message(sockfd, msg, 4);
	while(1) {
		printf("Enter a message of maxlen:%d chars\n", MAXLEN);

		char buf[MAXLEN];
		if (pa_simple_read(sr, buf, sizeof(buf), &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			goto finish;
		}

		buf[MAXLEN - 1] = '\0';

		/* if (pa_simple_write(s, buf, sizeof(buf), &error) < 0) { */
		/* 	fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error)); */
		/* 	goto finish; */
		/* } */
		send_message(sockfd, buf, MAXLEN);
//		printf("Message is %s and length is %lu\n", buf, strlen(buf));
	}
	close(sockfd);
	return NULL;

finish:
	if (sr)
		pa_simple_free(sr);
	return NULL;
}

void send_message(int sockfd, char *msg, int msg_len) {

	int bytes_sent = send(sockfd, msg, msg_len, 0);

	if(bytes_sent == -1) {
		perror("send");
		exit(1);
	}

	if(bytes_sent < msg_len) {
		printf("Only part of the message was sent :(\n");
	}
}

void *thread_read(void *param) {
	int sockfd;
	char msg[MAXLEN];
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	struct addrinfo hints, *res;
	int new_fd;

	// clear message
	memset(msg, 0, MAXLEN);

	/* // Execute close_server() signal handler when CTRL-C is called */
	/* if (signal(SIGINT, close_server) == SIG_ERR) { */
	/* 	perror("signal"); */
	/* 	exit(1); */
	/* } */

	// Load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use IPV4 or IPV6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me

	int ginfo = getaddrinfo(NULL, CPORT, &hints, &res);

	if(ginfo != 0) {
		// TODO: Make error statement more clear by including
		// the type of error. Check getaddrinfo man. Use errno
		perror("getaddrinfo");
		exit(1);
	}

	// make a socket, bind it, and listen on it:
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1) {
		perror("socket");
		exit(1);
	}

	// To reuse port and to avoid Bind: Already in use error
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	// Now accept an incoming connection
	addr_size = sizeof(their_addr);
	while(1) {
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

		if (new_fd == -1) {
			perror("accept");
			exit(1);
		}
		pthread_t thread_var;

		if(pthread_create(&thread_var, NULL, thread_routine, &new_fd)) {
			perror("pthread_create");
			exit(1);
		}

	}
	close(sockfd);
	return NULL;
}


void *thread_routine(void *param) {
	printf("Inside the thread\n");

	int error;
	char msg[MAXLEN];
	int new_fd = *(int *)param;

	printf("Process ID of this client: %d\n", getpid());
	int recv_ret = recv(new_fd, msg, MAXLEN - 1, 0);
	msg[MAXLEN - 1] = '\0';

	if(recv_ret == -1) {
		perror("recv");
		exit(1);
	}

	if(recv_ret == 0) {
		printf("Client %d closed the connection\n", getpid());
	}

        /* ... and play it */
        if (pa_simple_write(sw, msg, sizeof(msg), &error) < 0) {
		fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
		goto finish;
        }

	close(new_fd);
	return NULL;

finish:
	if (sw)
		pa_simple_free(sw);
	return NULL;
}
