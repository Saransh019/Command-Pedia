#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_CLIENTS 100
#define max_length 2000
#define BUFFER_SZ 4000

/* Client structure */
typedef struct {
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
	char mode;
} client_struct;

client_struct *clients[MAX_CLIENTS];

static _Atomic unsigned int client_no = 0;
static int uid = 10;

char *ip = "127.0.0.1";

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
	printf("\r%s", "> ");
	fflush(stdout);
}

void print_client_addr(struct sockaddr_in addr) {
	printf("%d.%d.%d.%d",
	       addr.sin_addr.s_addr & 0xff,
	       (addr.sin_addr.s_addr & 0xff00) >> 8,
	       (addr.sin_addr.s_addr & 0xff0000) >> 16,
	       (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to queue */
void add_client(client_struct *cl) {
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (!clients[i]) {
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void remove_client(int uid) {
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (clients[i]) {
			if (clients[i]->uid == uid) {
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

bool checkMode(char name[]) {
	if (name[0] != 'm' || name[1] != 'o' || name[2] != 'd' || name[3] != 'e' || name[4] != ':') {
		return false;
	}
	if (name[5] == '\0') {
		return false;
	}
	if((name[5]=='A' || name[5]=='G' || name[5]=='I') && name[6]=='\0'){
		return true;
	}
	return false;
}

int find_topic(char* topic, char* info){
	FILE* filePointer;
    int bufferLength = 255;
    char buffer[bufferLength];
    filePointer = fopen("index/index.txt", "r");
    int is_present=0;
    while(fgets(buffer, bufferLength, filePointer)) {
        char st[1000]="";
        buffer[strcspn(buffer, "\n")] = 0;
        strcat(st,buffer);

        if(strcmp(st,topic)==0) {
            is_present=1;
            FILE* filePointer1;
            
            strcat(st,".txt");
            char* tmp = strdup(st);
            strcpy(st,"topics/");
            strcat(st,tmp);
            free(tmp)
;
            filePointer1 = fopen(st, "r");
            while(fgets(info, BUFFER_SZ, filePointer1)) {
                info[strcspn(info, "\n")] = 0;
                
            }
            fclose(filePointer1);
        }
    }
    fclose(filePointer);
    if(is_present==0){
        printf("Topic is not yet present.\n");
    	return 0;
    }
    return 1;	
}

int ques_handle(char example_file[]){
    char dummy[1000]={};
    printf("Reading questions in Bulk:\n\n");

    char delim[] = ";";
    char* ptr = strtok(example_file,delim);
    int x = find_topic(ptr,&dummy);
    if(x==1){
    	return 0;
    }
    FILE* fptr;
    fptr = fopen("index/index.txt","a");

    char st[1000] = "topics/";
    strcat(st,ptr);
    strcat(st,".txt");
    fprintf(fptr,"%s\n",ptr);
    ptr=strtok(NULL,delim);
    fclose(fptr);


    FILE* tptr;
    tptr = fopen(st,"w");
    fprintf(tptr,"%s",ptr);
    fclose(tptr);
    return 1;

}

/* Send message to all clients except sender */
void send_msg(char *s, int uid) {
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (clients[i]) {
			if (clients[i]->uid != uid) {
				if (send(clients[i]->sockfd, s, strlen(s), 0) < 0) {
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void readHeader(char* header, char* msgtype, char* msg) {
	char* token = strtok(header, ":");
	strcpy(msgtype, token);
	token = strtok(NULL, ":");
	strcpy(msg, token);
}

/* Handle all communication with the client */
void *handle_client(void *args) {
	char out_buffer[BUFFER_SZ] = {};
	char info[BUFFER_SZ]={};
	char msg[BUFFER_SZ] = {};
	char msgtype[12] = {};
	char name[32];

	int exit = 0;
	client_no++;
	client_struct *cli = (client_struct *)args;

	// Name
	int n = recv(cli->sockfd, out_buffer, BUFFER_SZ, 0);

	readHeader(out_buffer, msgtype, msg);
	printf("msgtype: %s\nmsg: %s\n", msgtype, msg);
	if (n < 0 || strlen(msg) <  2 || strlen(msg) >= 32 - 1) {
		printf("Didn't enter the name.\n");
		exit = 1;
	} else if (strcmp(msgtype, "ID") == 0) {
		strcpy(cli->name, msg);
		sprintf(out_buffer, "\nWelcome %s to the command line encyclopedia \n\nSelect mode:\npress I for user mode\npress A for admin mode\n\nformat-> mode:<type of mode>\n", cli->name);
		send(cli->sockfd, out_buffer, BUFFER_SZ, 0);
		bzero(out_buffer, BUFFER_SZ);
		int receive = recv(cli->sockfd, out_buffer, BUFFER_SZ, 0);
		if (receive < 0) {
			printf("ERROR:-1\n");
			exit = 1;
		}
		while(!checkMode(out_buffer)){
			bzero(out_buffer,BUFFER_SZ);
			sprintf(out_buffer, "%s\n","Enter valid mode format.");
			send(cli->sockfd,out_buffer,BUFFER_SZ,0);
			bzero(out_buffer,BUFFER_SZ);
			recv(cli->sockfd, out_buffer, BUFFER_SZ, 0);
		}
		cli->mode = out_buffer[5];		
	}
	readHeader(out_buffer, msgtype, msg);
	printf("%s has selected %s mode\n", cli->name, msg);
	bzero(out_buffer, BUFFER_SZ);
	char buff_topic[BUFFER_SZ];
	
	if (strcmp(msgtype, "mode") == 0 && strcmp(msg, "I") == 0) {
		while (1) {
			if (exit) {
				break;
			}

		N:	sprintf(out_buffer, "Ok. Type a topic to search:");
			send(cli->sockfd, out_buffer, BUFFER_SZ, 0);
			bzero(out_buffer, BUFFER_SZ);
			bzero(buff_topic, BUFFER_SZ);
			int receive = recv(cli->sockfd, buff_topic, BUFFER_SZ, 0);
			printf("'%s'\n", buff_topic);
				if (receive > 0) {
						if (strlen(out_buffer) > 0 || strlen(buff_topic) > 0) {
							
							int x = find_topic(buff_topic, &info);
							bzero(out_buffer, BUFFER_SZ);
							if (x == 0) {
								sprintf(out_buffer, "%s\n", "Sorry, Topic not present");
								send(cli->sockfd, out_buffer, BUFFER_SZ, 0);
							}
							//send ques

							else {
								printf("topic info: %s\n", info);
								send(cli->sockfd, info, BUFFER_SZ, 0);
								bzero(info,BUFFER_SZ);	
							}

							bzero(out_buffer, BUFFER_SZ);
							sprintf(out_buffer, "\n%s\n", "To search another topic press'n'\nTo quit press 'q'\n");
							send(cli->sockfd, out_buffer, BUFFER_SZ, 0);
							bzero(out_buffer, BUFFER_SZ);
							recv(cli->sockfd, out_buffer, BUFFER_SZ, 0);
							//quit
							if (strcmp(out_buffer, "q") == 0) {
								//exit=1
								printf("%s has aked to quit\n",cli->name);
								exit = 1;
							}
							//next ques
							else if (strcmp(out_buffer, "n") == 0) {
								//return to Q
								printf("%s has asked for next ques\n",cli->name);
								goto N;
							}
							
							
						}

					} else if (receive == 0 || strcmp(out_buffer, "q") == 0) {
						sprintf(out_buffer, "%s has left\n", cli->name);
						printf("%s", out_buffer);
						exit = 1;
					} else {
						printf("ERROR: -1\n");
						exit = 1;
					}

					bzero(out_buffer, BUFFER_SZ);
				}
/* Delete client from queue and yield thread */
	close(cli->sockfd);
	remove_client(cli->uid);
	free(cli);
	client_no--;
	pthread_detach(pthread_self());
	}	


//Admin mode handling
	if (strcmp(msgtype, "mode") == 0 && strcmp(msg, "A") == 0) {
		while (1) {
			if (exit) {
				break;
			}
Q:	bzero(out_buffer, BUFFER_SZ);
			sprintf(out_buffer, "%s\n", "you are in ADMIN mode. You can add topic in the following format:\n<topic> ; <info>");
			send(cli->sockfd, out_buffer, BUFFER_SZ, 0);
			bzero(out_buffer, BUFFER_SZ);
			int receive = recv(cli->sockfd, out_buffer, BUFFER_SZ, 0);

			if (receive > 0) {

				if (strlen(out_buffer) > 0) {
					int x = ques_handle(out_buffer);
					
					if(x==0){
						bzero(out_buffer, BUFFER_SZ);
						sprintf(out_buffer, "%s\n", "Topic already present\n\nPress q to quit\nPress n to add more topics\n");
						send(cli->sockfd, out_buffer, BUFFER_SZ, 0);
					
					}
					else{
						bzero(out_buffer, BUFFER_SZ);
						sprintf(out_buffer, "%s\n", "Topic added successfully!!! \n\nPress q to quit\nPress n to add more topics\n");
						send(cli->sockfd, out_buffer, BUFFER_SZ, 0);
					}
					bzero(out_buffer, BUFFER_SZ);
					recv(cli->sockfd, out_buffer, BUFFER_SZ, 0);

					
					if (strcmp(out_buffer, "q") == 0) {
						printf("%s has asked to quit\n",cli->name);
						exit = 1;
					}
					else if (strcmp(out_buffer, "n") == 0) {
						printf("%s has asked to write new ques\n",cli->name);
						goto Q;
					}
				}
			}
			else if (receive == 0 || strcmp(out_buffer, "q") == 0) {
				sprintf(out_buffer, "%s has left\n", cli->name);
				printf("%s\n", out_buffer);
				exit = 1;
			}
			else {
				printf("ERROR: -1\n");
				exit = 1;
			}

			bzero(out_buffer, BUFFER_SZ);

		}
		/* Delete client from queue and yield thread */
		close(cli->sockfd);
		remove_client(cli->uid);
		free(cli);
		client_no--;
		pthread_detach(pthread_self());

	}

	return NULL;
}	

int main() {

	int port = 5033;
	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (char*)&option, sizeof(option)) < 0) {
		perror("ERROR: setsockopt failed");
		return EXIT_FAILURE;
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0) {
		perror("ERROR: setsockopt failed");
		return EXIT_FAILURE;
	}

	/* Bind */
	if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

	/* Listen */
	if (listen(listenfd, 10) < 0) {
		perror("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}

	printf("****** WELCOME TO THE ENCYCLOPEDIA ******\n");

	while (1) {
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if ((client_no + 1) == MAX_CLIENTS) {
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}	

		/* Client settings */
		client_struct *cli = (client_struct *)malloc(sizeof(client_struct));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		add_client(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}
