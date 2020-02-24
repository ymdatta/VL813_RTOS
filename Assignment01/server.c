#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// #define MYPORT "3233"
char MYPORT[4];  //the port clients will be connecting to
#define BACKLOG 5 // how many pending client connections queue will hold
#define MAXLEN 1000

/* A client structure
 * g_id - Group ID of the client
 * port[4] - Receiving port of the client.
 *        (The port to which server should send messages to client)
 * next - Pointer to the next client.
 */	  	  
struct client {
	int g_id;
	char port[4];
	struct client *next;
};

void close_server(int signum);
void *thread_recv_msg_from_client(void *param);
void send_message(int g_id, char *msg, char* s_port, clock_t start_time);
void add_client(struct client *temp);

// sockfd a global variable so that it can be closed
// in a signal handler when a signal is received.
int sockfd;

// g_sockets points to the head of linkedlist containing client structures
struct client *g_sockets = NULL;

int main(int argc, char **argv) {

	if (argc != 2) {
		printf("Usage: ./server PORT_NO\n");
		printf("PORT_NO: Port at which server listens to messages from clients\n");
		printf("Ex: ./server 3233\n");
		exit(1);
	}

	memset(MYPORT, 0, 4);
	strcpy(MYPORT, argv[1]);
	
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

		if(pthread_create(&thread_var, NULL, thread_recv_msg_from_client, &new_fd)) {
			perror("pthread_create");
			exit(1);
		}
	       
	}
	close(sockfd);
	return 0;
}


void *thread_recv_msg_from_client(void *param) {
	char msg[MAXLEN];
	int new_fd = *(int *)param;
	int flag = 2;
	struct client *s_new = malloc(sizeof(struct client));
	s_new->g_id = 1;
	// TODO: Explain why we need this varialbe in comments
	// In Brief: to limit what recv receives.
	int r_len = 1;
	while(1) {
		memset(msg, 0, MAXLEN);								
		int recv_ret = recv(new_fd, msg, r_len, 0);

		if(recv_ret == -1) {
			perror("recv");
			exit(1);
		}

		if(recv_ret == 0) {
			printf("Client closed the connection\n");
			break;
		}
		if (flag == 2) {
			printf("Message received is %s\n", msg);
			printf("GroupId of connection: %d\n", atoi(msg));
			s_new->g_id = atoi(msg);
			r_len = MAXLEN;
		}
		else if(flag == 1) {
			printf("Port number is %s\n", msg);
			memset(s_new->port, 0, 4);
			strncpy(s_new->port, msg, 4);
			s_new->next = NULL;
			add_client(s_new);
		} else {
			printf("Message received by server: %s\n", msg);
			clock_t start_time;
			if((start_time = clock()) == -1) {
				perror("start_time: clock");
				exit(1);
			}
			send_message(s_new->g_id, msg, s_new->port, start_time);
		}

		if (flag > 0)
			flag--;
	}
	close(new_fd);
}

/* This function closes the server socket and prints all the clients
 * which were connected to it before.
 */
void close_server(int signum) {
	char c;
	printf("Exit Y/N?\n");
	while(1) {
		c = getchar();
		if ( c == 'Y' | c == 'N')
			break;
	}

	if(c == 'Y') {
		printf("Log of all clients connected to the server:\n");		
		close(sockfd);
		struct client *temp = g_sockets;
		while(temp != NULL) {
			struct client *a = temp;
			printf("id:  %d, PORT: %s\n", a->g_id, a->port);
			temp = temp->next;
			free(a);
		}
		exit(0);
	} else {
		return;
	}
}

/* This function sends messages to client.
 * g_id - The group to which the message is to be sent
 * msg - The message to be sent
 * s_port - The sender's port, this is used to send the message to everyone
 *          but the sender in a group.
 * start_time - Time at which send_message call was issued.
 */
void send_message(int g_id, char* msg, char* s_port, clock_t start_time) {

	int sockfd;
	char cPort[4];
	clock_t end_time;
	struct addrinfo hints, *res;	
	struct client *c_node = g_sockets;

	// Load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use IPV4 or IPV6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me

	while(c_node != NULL) {
		// Send messages to clients of the same group.
		if(c_node->g_id == g_id) {

			// Send messages to other than sender in the group
			// We identify the sender using his receiving port number. (s_port)
			if(strncmp(s_port, c_node->port, 4)) {

				strncpy(cPort, c_node->port, 4);

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

				// send the message now.
				int bytes_sent = send(sockfd, msg, msg_len, 0);

				if(bytes_sent == -1) {
					perror("send");
					exit(1);
				}

				if(bytes_sent < msg_len) {
					printf("Only part of the message was sent :(\n");
				}
			}

		}
		c_node = c_node->next;
	}

	if((end_time = clock()) == -1) {
		perror("end_time: clock");
		exit(1);
	}

	double dur = 1000.0 * (end_time - start_time) / CLOCKS_PER_SEC;
	printf("CPU time used (per clock()): %.2f ms\n", dur);
	close(sockfd);
}

// Adds a new client to the linkedlist
void add_client(struct client *n_client) {
	struct client **head_t = &g_sockets;
	struct client *head = *head_t;

	if(head == NULL) {
		*head_t = n_client;
	} else {
		while(head->next != NULL) {
			if(!strncmp(n_client->port, head->port, 4)) {
				free(n_client);
				return;
			}
			head = head->next;
		}
		head->next = n_client;
	}
	return;
}
