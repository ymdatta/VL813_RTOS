// TODO: Rearrange the headers alphabetically
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define MYPORT "3233" //the port clients will be connecting to
#define BACKLOG 5 // how many pending client connections queue will hold
#define MAXLEN 1000

struct s_list {
	int id;
	char s[4];
	struct s_list *next;
};

struct s_list *g_sockets = NULL;

void add_socket(struct s_list *temp) {

	// TODO: Fix this
	struct s_list **head_t = &g_sockets;
	struct s_list *head = *head_t;

	if(head == NULL) {
		*head_t = temp;
	} else {
		while(head->next != NULL) {
			if(!strncmp(temp->s, head->s, 4)) {
				free(temp);
				return;
			}
			head = head->next;
		}
		head->next = temp;
	}
	return;
}
	

// sockfd a global variable so that it can be closed
// in a signal handler when a signal is received.
int sockfd;

void close_server(int signum);
void *thread_routine(void *param);

void send_message(int g_id, char *msg);

int main(void) {
	char msg[MAXLEN];
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	struct addrinfo hints, *res;
	int new_fd;

	// clear message
	memset(msg, 0, MAXLEN);

	// Execute close_server() signal handler when CTRL-C is called
	if (signal(SIGINT, close_server) == SIG_ERR) {
		perror("signal");
		exit(1);
	}
	
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
	return 0;
}
void *thread_routine(void *param) {
	char msg[MAXLEN];
	int new_fd = *(int *)param;
	int flag = 2;
	struct s_list *s_new = malloc(sizeof(struct s_list));	
	while(1) {
		memset(msg, 0, MAXLEN);								
		int recv_ret = recv(new_fd, msg, MAXLEN, 0);

		if(recv_ret == -1) {
			perror("recv");
			exit(1);
		}

		if(recv_ret == 0) {
			printf("Client %d closed the connection\n", getpid());
			break;
		}
		if (flag == 2) {
			printf("In here with flag..\n");
			printf("GroupId of connection: %d\n", atoi(msg));
			s_new->id = atoi(msg);
		}
		else if(flag == 1) {
			printf("Too here with flag..\n");			
			printf("Port number is %s\n", msg);
			memset(s_new->s, 0, 4);
			strncpy(s_new->s, msg, 4);
			s_new->next = NULL;
			add_socket(s_new);
		} else {
			printf("Message received by server: %s\n", msg);
			send_message(1, msg);
		}

		if (flag > 0)
			flag--;
	}
	close(new_fd);	
}
								      
void close_server(int signum) {
	char c;
	printf("Exit Y/N?\n");
	while(1) {
		c = getchar();
		if ( c == 'Y' | c == 'N')
			break;
	}

	if(c == 'Y') {
		close(sockfd);
		struct s_list *temp = g_sockets;
		while(temp != NULL) {
			struct s_list *a = temp;
			printf("id:  %d, PORT: %s\n", a->id, a->s);
			temp = temp->next;
			free(a);
		}
		exit(0);
	} else {
		return;
	}
}
	        
void send_message(int g_id, char* msg) {

	int sockfd;
	struct addrinfo hints, *res;
	// Load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use IPV4 or IPV6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me

	char cPort[4];
	struct s_list *node = g_sockets;
	while(node != NULL) {
		if(node->id == g_id) {
			strncpy(cPort, node->s, 4);

			int ginfo = getaddrinfo(NULL, cPort, &hints, &res);

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
			freeaddrinfo(res);
			while(connect(sockfd, res->ai_addr, res->ai_addrlen) != -1) {
				perror("connect");
			};

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
		node = node->next;
	}
	close(sockfd);
}
