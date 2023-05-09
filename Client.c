#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#define length 4000
volatile sig_atomic_t sigflag = 0;
char username[32];
int sockfd=0;
char *ip = "127.0.0.1";
int port = 5033;

void catch_ctrl_c_and_exit(int sig) {
	sigflag = 1;
}

void getinput(char *store, int len) {
	memset(store, 0, len);        
	fgets(store, len, stdin);
	for (int i = 0; i < len; i++) { // trim \n
		if (store[i] == '\n') {
			store[i] = '\0';
			break;
		}
	}
	return ;
}

bool checkUsername() {
	if (username[0] != 'I' || username[1] != 'D' || username[2] != ':') {
		return false;
	}
	if (username[3] == '\0') {
		return false;
	}
	return true;
}

int Connect(){
	char msg[length] = {};
	struct sockaddr_in server_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);
	
	int error = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	
	if (error == -1) {
		printf("ERROR: connecting to Server\n");
		return EXIT_FAILURE;
	}
	
	if (send(sockfd, username, 32, 0) < 0) {
		printf("Error:sending message\n");
		return EXIT_FAILURE;
	}
	
	recv(sockfd, msg, length, 0);
	printf("%s\n", msg);
	return -1;
}

void msg_send_helper() {
	char msg[length] = {};
	char buffer[length + 32] = {};
	while (1) {
		printf("%s", "> ");
		fflush(stdout);

		getinput(msg, length);
		if (strcmp(msg, "q") == 0) {
			break;
		}
		else {
			sprintf(buffer, "%s", msg);
			send(sockfd, buffer, strlen(buffer), 0);
		}

		bzero(msg, length);
		bzero(buffer, length + 32);
	}
	catch_ctrl_c_and_exit(2);
}

void msg_recv_helper() {
	char msg[length] = {};
	while (1) {
		int receive = recv(sockfd, msg, length, 0);
		if (receive > 0) {
			printf("%s", msg);
			printf("%s", "> ");
			fflush(stdout);
		} else if (receive == 0) {
			break;
		}
		memset(msg, 0, sizeof(msg));
	}
}

int main(){
	signal(SIGINT, catch_ctrl_c_and_exit);
	printf("To connect with the server please enter your 32-bit name as ID:<name>\n");
	getinput(username, 32);
	
	while (!checkUsername()) {
		printf("Please enter your name in valid format 'ID:<name>'\n");
		bzero(username, 32);
		getinput(username, 32);
	}
	
	int sockfd;
	sockfd = Connect(); //connecting to the server
	
	pthread_t msg_send_thread;
	if (pthread_create(&msg_send_thread, NULL, (void *) msg_send_helper, NULL) != 0) {
		printf("ERROR: pthread send\n");
		return EXIT_FAILURE;
	}
	
	pthread_t msg_recv_thread;
	if (pthread_create(&msg_recv_thread, NULL, (void *) msg_recv_helper, NULL) != 0) {
		printf("ERROR: pthread receive\n");
		return EXIT_FAILURE;
	}
	
	while (true) {
		if (sigflag) {
			printf("Exited Successfully\n");
			break;
		}
	}
	close(sockfd);
	return EXIT_SUCCESS;
}
