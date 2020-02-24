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

// #define SPORT "3233"
char SPORT[4];  //the port clients will be connecting to
#define BACKLOG 5 // how many pending client connections queue will hold
#define MAXLEN 1000

// #define CPORT "8001" // Port where client receives messages

char CPORT[100];
// default ID is 0
char ID[4];

void *thread_read(void *param);
void *thread_write(void *param);
void *thread_routine(void *param);
void send_message(int sockfd, char *msg);

int main(int argc, char **argv) {

	if (argc != 4) {
		printf("Usage: ./client GROUP_ID CLIENT_PORT_NO SERVER_PORT_NO\n");
		printf("GROUPID: Group to which this client belongs\n");
		printf("CLIENT_PORT_NO: Port at which client receives messages\n");
		printf("SERVER_PORT_NO: Port at which server receives messages (4 digits)\n");
		printf("Ex: ./client 2 4001 3233\n");
		exit(1);
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
}

/*
 * Protocol: When client connects to server. It does
 * 1. Send it's group id
 * 2. Send it's receiving PORT to server
 * 3. Enter while loop
 * 4. Take input from user and send to server
 * 5. Terminate when message sending fails (or) server closes
 */

// TODO: Implement GroupID protocol along with port
void *thread_write(void *param) {
	char msg[1000];

	int sockfd;
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
	send_message(sockfd, msg);
	memset(msg, 0, MAXLEN);		
	strncpy(msg, CPORT, 4);
	send_message(sockfd, msg);
	while(1) {
		printf("Enter a message of maxlen:%d chars\n", MAXLEN);
		if (fgets(msg, MAXLEN, stdin) == NULL) {
			perror("fgets");
			exit(1);
		}

		send_message(sockfd, msg);
	}

	close(sockfd);
}

void send_message(int sockfd, char *msg) {
	printf("Sending message %s\n", msg);
	// casting done here. Be careful. Check again
	int msg_len = strlen(msg);
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
}

								      
void *thread_routine(void *param) {
	char msg[MAXLEN];
	int new_fd = *(int *)param;
	
	printf("Process ID of this client: %d\n", getpid());
	int recv_ret = recv(new_fd, msg, MAXLEN, 0);

	if(recv_ret == -1) {
		perror("recv");
		exit(1);
	}

	if(recv_ret == 0) {
		printf("Client %d closed the connection\n", getpid());
	}

	printf("Message received by %d: %s\n", getpid(), msg);
	memset(msg, 0, MAXLEN);

	close(new_fd);	
}
		
	
