/**********************************************
 * Jan Kuliga, Krzysztof Bera, Konrad Sikora
 * ********************************************/
/*
 *INCLUDES
 */
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h> 
#include <pthread.h> 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "strmap.h"
#include <sys/errno.h>
/*
 * MACROS
 */
#define SERV_PORT 27428
#define PEER_PORT 27429
#define MAX_RCV_LEN 2048
#define STR_LEN 2000

/*
 * FUNCTION PROTOTYPES
 */
void tic_tac_toe(int socket, char *buf, int player_id);

void *server_thread(void* par);

void *peer_thread(char *addr);

/*
 * STATIC VARIABLES
 */
enum {
		PEER,
		SERV,
		LIST,
		CONN
} act;

enum {
		MINE,
		OPP 
} result;

enum {
		PEER_,
		SERV_,
		QUASISERV_
} address;

static int res[2];

static int fd[4];

static char my_name[STR_LEN], opp_name[STR_LEN];

static pthread_t thr_id;

//static pthread_mutex_t sync_proc;

int main(int argc, char *argv[])
{
		struct sockaddr_in addr[3];
		socklen_t addr_len;
		char data_buffer[MAX_RCV_LEN];
		int port, len;
		
		/*turn off stdout buffering, doesn't have to be done on every OS;
		 * that can be harsh, really*/
		setvbuf(stdout, NULL, _IONBF, 0);
		
		/*mutex init*/
		//pthread_mutex_init(&sync_proc, NULL);
		
		/*reset sockaddr_in structures*/
		for (int i = 0; i < 3; i++) 
				memset((void*) &addr[i], 0, sizeof(addr[i]));
		
		addr[SERV_].sin_family = AF_INET;

		if (inet_pton(AF_INET, argv[1], &addr[SERV_].sin_addr) <= 0) {
				fprintf(stderr,"Address error: inet_pton error for %s : %s \n", argv[1], strerror(errno));
				exit(1);
		}
		
		/*Check whether port was specified during server start-up*/
		if (argc > 2) 
				port = atoi(argv[2]);
		else 
				port = SERV_PORT;
		
		/*Check whether specified port is valid*/
		if (port > 0) {
				addr[SERV_].sin_port = htons(port);
		} else {
				fprintf(stderr, "Invalid port number. Shutting down client...\n");
				exit(1);
		}
		
		if ((fd[SERV] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				fprintf(stderr, "Socket failure. Shutting down client...\n");
				exit(1);
		}
		
		if (connect(fd[SERV], (struct sockaddr*) &addr[SERV_], sizeof(struct sockaddr)) < 0)
				fprintf(stderr, "Critical failure during connect.\n");
		
		/*handle communication with server in seperate thread*/
		pthread_create(&thr_id, NULL, server_thread, (void*) &fd[SERV]);
		
		/*<Invite>*/
		addr_len = sizeof(struct sockaddr_in);
		
		addr[QUASISERV_].sin_family = AF_INET;
		addr[QUASISERV_].sin_addr.s_addr = htonl(INADDR_ANY);
		addr[QUASISERV_].sin_port = htons(PEER_PORT);
		
		if ((fd[LIST] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				fprintf(stderr, "Socket failure. Shutting down client...\n");
				exit(1);
		}
		
		/*Bind a adress to the socket*/
		if (bind(fd[LIST], (struct sockaddr*) &addr[QUASISERV_], sizeof(addr[QUASISERV_])) < 0) {
				fprintf(stderr, "Critical failure when binding address. Shutting down client...\n");
				exit(1);
		}
		
		/*accept only one invite query*/
		if (listen(fd[LIST], 1) < 0) {
				fprintf(stderr, "Critical failure during listen(). Shutting down client...\n");
				exit(1);
		}
		
		if ((fd[CONN] = accept(fd[LIST], (struct sockaddr*) &addr[PEER], &addr_len) < 0)) {
				fprintf(stderr, "Critical failure during accepting request.\n");
				exit(1);
		}
		
		printf("Up and running...\n");
		
		while (fd[CONN]) {
				/*server thread reads constantly from input stream,
				 * in order to redirect it to peer you have to cancel that thread,
				 * connection with server is still established though*/
				pthread_cancel(thr_id);
				
				len = recv(fd[CONN], data_buffer, MAX_RCV_LEN, 0);
				data_buffer[len] = '\0';
				strcpy(opp_name, data_buffer);
				printf("\n%s invited you. Accept challenge (y/n) ?", opp_name);
				
				/*approve*/
				if (strcmp(data_buffer, "y") == 0) {
						res[MINE] = 0;
						res[OPP] = 0;
						/*TODO: logic*/
				} else {
						res[MINE] = 0;
						res[OPP] = 0;
						/*TODO: logic*/
				}
				
				/*TODO: game logic, cancel connection until one of players declines*/
				
				
				
				/*after disconnect, recreate thread to start communication
				 * with server and keep socket's fd open for other peers*/
				res[MINE] = 0;
				res[OPP] = 0;
				opp_name[0] = '\0';
				pthread_create(&thr_id, NULL, server_thread, (void*) &fd[SERV]);
				if ((fd[CONN] = accept(fd[LIST], (struct sockaddr*) &addr[PEER], &addr_len) < 0)) {
						fprintf(stderr, "Critical failure during accepting request.\n");
						exit(1);
				}
		}
		/*TODO: close all sockets*/

}

void *server_thread(void *par)
{
		short flag = 0;
		char data_buffer[MAX_RCV_LEN + 1];
		int len, n;
		char arg1[STR_LEN], arg2[STR_LEN];
		/*set cancel state so that it can be cancelled outside this function*/
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		
		/* Print use instructions */
		printf("\nUsage: \n\n join {name}\n list\n invite {player}\n leave\n\n");
		
		/*loop used to communicate with server; runs until thread is cancelled
		 * or used entered <leave> query*/
		while (1) {
				fgets(data_buffer, sizeof(data_buffer), stdin);
				data_buffer[strlen(data_buffer) - 1] = '\0';
				n = sscanf(data_buffer, "%s %s", arg1, arg2);
				
				if ((strcmp(arg1, "join") == 0) && n > 1) {
						if (strlen(my_name) < 1) {
								send(fd[SERV], data_buffer, strlen(data_buffer), 0);
								len = recv(fd[SERV], data_buffer, MAX_RCV_LEN, 0);
								data_buffer[len] = '\0';
								printf("\n%s\n", data_buffer);
								strcpy(my_name, arg2);
						} else {
								printf("You've set your nickname before! Nick: %s\n", my_name);
						}
				} else if ((strcmp(arg1, "invite") == 0) && n > 1) {
						if (strlen(my_name) < 1) {
								printf("Join the game first...\n");
						} else {
								strcpy(opp_name, arg2);
								send(fd[SERV], data_buffer, strlen(data_buffer), 0);
								len = recv(fd[SERV], data_buffer, MAX_RCV_LEN, 0);
								data_buffer[len] = '\0';
								pthread_create(&thr_id, NULL, (void*) peer_thread, data_buffer);
								pthread_exit(0);
						}
				} else if (strcmp(arg1, "list") == 0) {
						send(fd[SERV], data_buffer, strlen(data_buffer), 0);
						len = recv(fd[SERV], data_buffer, MAX_RCV_LEN, 0);
						data_buffer[len] = '\0';
						printf("\n%s\n", data_buffer);
				} else if (strcmp(arg1, "leave") == 0) {
						my_name[0] = '\0';
						printf("\nGOODBYE!\n");
						close(fd[MINE]);
						close(fd[SERV]);
						close(fd[PEER]);
						close(fd[CONN]);
						exit(1);
				} else {
						if (!flag)
								flag = 1;
						else
								printf("\nUsage: \n\n join {name}\n list\n invite {player}\n leave\n\n");
				}
		}
}

void *peer_thread(char *addr)
{
		char data_buffer[MAX_RCV_LEN + 1]; 
		int len;
		struct sockaddr_in peer_addr;
		memset(&peer_addr, 0, sizeof(peer_addr));
		if (inet_pton(AF_INET, addr, &peer_addr.sin_addr) <= 0) {
				fprintf(stderr,"Address error: inet_pton error for %s : %s \n", addr, strerror(errno));
				exit(1);
		}
		peer_addr.sin_family = AF_INET;
		peer_addr.sin_port = htons(PEER_PORT);
		
		if ((fd[PEER] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				fprintf(stderr, "Socket failure. Shutting down client...\n");
				exit(1);
		}
		
		printf("Sending invite\n");
		
		if (connect(fd[PEER], (struct sockaddr*) &addr[PEER], sizeof(struct sockaddr)) < 0)
				fprintf(stderr, "Critical failure during connect.\n");
		
		send(fd[PEER], my_name, strlen(my_name), 0);
		len = recv(fd[PEER], data_buffer, MAX_RCV_LEN, 0);
		data_buffer[len] = '\0';
		if (strcmp(data_buffer, "y") == 0) {
				/*TODO: LOGIC*/
				printf("YES\n");
		} else {
				/*TODO: LOGIC*/
				printf("NO\n");
		}
		/*TODO: LOGIC*/
		opp_name[0] = '\0';
		close(fd[PEER]);
		pthread_create(&thr_id, NULL, server_thread, &fd[SERV]);
		pthread_exit(0);
}

















