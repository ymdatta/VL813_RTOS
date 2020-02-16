// TODO: Rearrange the headers alphabetically
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define MYPORT "3233" //the port clients will be connecting to
#define BACKLOG 5 // how many pending client connections queue will hold
#define MAXLEN 1000

int main(void) {
	char msg[1000];
	struct addrinfo hints, *res;
	int sockfd;

	// Load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use IPV4 or IPV6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me

	int ginfo = getaddrinfo(NULL, MYPORT, &hints, &res);

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

	while(1) {

		printf("Enter a message of maxlen:%d chars\n", MAXLEN);
		if (fgets(msg, MAXLEN, stdin) == NULL) {
			perror("fgets");
			exit(1);
		}

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

	close(sockfd);
	return 0;
}

								      

		
	
