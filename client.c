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
#define SERV_PORT 22222
#define PEER_PORT 22223
#define MAX_RCV_LEN 2048
#define STR_LEN 2000

/*
 * FUNCTION PROTOTYPES
 */
 
void *server_thread(void* par);

void *peer_thread(char *addr);

unsigned short tic_tac_toe(int socket, char *buf, int player_id);

void draw_board(char* board);

unsigned short check_win(char* board);

unsigned short check_move(char*board, int choice);

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

int main(int argc, char *argv[])
{
	struct sockaddr_in addr[3];
	socklen_t addr_len;
	char data_buffer[MAX_RCV_LEN];
	int port, len;
	int player_accpt;
	
	/*turn off stdout buffering, doesn't have to be done on every OS*/
	setvbuf(stdout, NULL, _IONBF, 0);
	
	/*reset sockaddr_in structures*/
	for (int i = 0; i < 3; i++) 
		memset((void*) &addr[i], 0, sizeof(addr[i]));
	
	addr[SERV_].sin_family = AF_INET;

	if (argc == 1) {
		printf("Server's address not specified. Shutting down...\n");
		goto exit;
	} else if (inet_pton(AF_INET, argv[1], &addr[SERV_].sin_addr) <= 0) {
		fprintf(stderr,"Address error: inet_pton error for %s : %s \n", argv[1], strerror(errno));
		goto exit;
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
		goto exit;
	}
	
	if ((fd[SERV] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Socket failure. Shutting down client...\n");
		goto exit;
	}
	
	if (connect(fd[SERV], (struct sockaddr*) &addr[SERV_], sizeof(addr[SERV_])) < 0) {
		fprintf(stderr, "Critical failure during connect.\n");
		goto exit;
	}

	/*handle communication with server in seperate thread*/
	pthread_create(&thr_id, NULL, server_thread, (void*) &fd[SERV]);
	
	/*Invite*/
	addr_len = sizeof(struct sockaddr_in);
	
	addr[QUASISERV_].sin_family = AF_INET;
	addr[QUASISERV_].sin_addr.s_addr = htonl(INADDR_ANY);
	addr[QUASISERV_].sin_port = htons(PEER_PORT);
	
	if ((fd[LIST] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Socket failure. Shutting down client...\n");
		goto exit;
	}
	
	/*Bind a adress to the socket*/
	if (bind(fd[LIST], (struct sockaddr*) &addr[QUASISERV_], sizeof(addr[QUASISERV_])) < 0) {
		fprintf(stderr, "Critical failure when binding address. Shutting down client...\n");
		goto exit;
	}
	
	/*accept only one invite request*/
	if (listen(fd[LIST], 1) < 0) {
		fprintf(stderr, "Critical failure during listen(). Shutting down client...\n");
		goto exit;
	}
	
	if ((fd[CONN] = accept(fd[LIST], (struct sockaddr*) &addr[PEER_], &addr_len)) < 0) {
		fprintf(stderr, "Critical failure during accepting request.\n");
		goto exit;
	}
	
	printf("Up and running...\n");
	
	while (fd[CONN]) {
		/*server thread reads constantly from input stream,
		in order to redirect it to peer you have to cancel that thread,
		connection with server is still established though*/
		pthread_cancel(thr_id);

		len = recv(fd[CONN], data_buffer, MAX_RCV_LEN, 0);
		data_buffer[len] = '\0';
		strcpy(opp_name, data_buffer);

		printf("\n\t%s invited you. Accept challenge (y/n)?", opp_name);
		
		fgets(data_buffer, sizeof(data_buffer), stdin);
		data_buffer[strlen(data_buffer) - 1] = '\0';
		
		/*approve*/
		if (strcmp(data_buffer, "y") == 0) {
			res[MINE] = 0;
			res[OPP] = 0;
			printf("\n\tACCEPTED\n");
			player_accpt=1;
			send(fd[CONN], data_buffer, strlen(data_buffer), 0);
		} else {
			res[MINE] = 0;
			res[OPP] = 0;
			printf("\n\tNOT ACCEPTED\n");
			player_accpt=0;
		}
		
		int player_id = 2;
		if(player_accpt)
			while(!tic_tac_toe(fd[CONN],data_buffer,player_id));

		/*after disconnect, recreate thread to start communication
		with server and keep socket's fd open for other peers*/
		res[MINE] = 0;
		res[OPP] = 0;
		opp_name[0] = '\0';
		pthread_create(&thr_id, NULL, server_thread, (void*) &fd[SERV]);
		if ((fd[CONN] = accept(fd[LIST], (struct sockaddr*) &addr[PEER_], &addr_len) < 0)) {
			fprintf(stderr, "Critical failure during accepting request.\n");
			goto exit;
		}
	}
exit:
	close(fd[PEER]);
	close(fd[SERV]);
	close(fd[LIST]);
	close(fd[CONN]);

}

void *server_thread(void *par)
{
	char data_buffer[MAX_RCV_LEN + 1];
	int len, n;
	char arg1[STR_LEN], arg2[STR_LEN];
	
	/*set cancel state so that it can be cancelled outside this function*/
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	printf("\n\tWelcome to the tic-tac-toe game!\n");
	printf("\n\tTo add your nick to server queue, type: join <your nick>");
	printf("\n\tTo see list of all players waiting in server queue, type: list");
	printf("\n\tTo invite a player to game, type: invite <nick of player you want to invite>");
	printf("\n\tTo leave a game, type: leave\n");
	
	/*loop used to communicate with server; runs until thread is cancelled
	or used entered <leave> query*/
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
			printf("\nGoodbye!\n");
			close(fd[MINE]);
			close(fd[SERV]);
			close(fd[PEER]);
			close(fd[CONN]);
			exit(1);
		} else {
			printf("\n\tIncorrect command was entered\n");
			printf("\n\tTo add your nick to server queue, type: join <your nick>");
			printf("\n\tTo see list of all players waiting in server queue, type: list");
			printf("\n\tTo invite a player to game, type: invite <nick of player you want to invite>");
			printf("\n\tTo leave a game, type: leave\n");
		}
	}
}

void *peer_thread(char *addr)
{
	char data_buffer[MAX_RCV_LEN + 1]; 
	int len;
	int player_accpt;
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
	
	printf("\n\tSending invite\n");
	
	if (connect(fd[PEER], (struct sockaddr*) &peer_addr, sizeof(peer_addr)) < 0) {
		fprintf(stderr, "Critical failure during connect.\n");
		exit(1);
	}
	
	send(fd[PEER], my_name, strlen(my_name), 0);
	len = recv(fd[PEER], data_buffer, MAX_RCV_LEN, 0);
	data_buffer[len] = '\0';
	if (strcmp(data_buffer, "y") == 0) {
		res[MINE] = 0;
		res[OPP] = 0;
		printf("\nChallenge accepted.\n");
		player_accpt=1;
	} else {
		res[MINE] = 0;
		res[OPP] = 0;
		printf("\n%s declined\n",opp_name);
		player_accpt=0;
	}
			
	int player_id=1;
	if(player_accpt)
		while(!tic_tac_toe(fd[PEER], data_buffer, player_id));

	opp_name[0] = '\0';
	close(fd[PEER]);
	pthread_create(&thr_id, NULL, server_thread, &fd[SERV]);
	pthread_exit(0);
}

/*return 0 if game is in progress
return 1 if player doesnt want to play
return 2 if opponent doesnt want to play*/
unsigned short tic_tac_toe(int socket, char *buf, int player_id)
{
	static int number_of_games = 0;
	number_of_games++;
	int data_socket = socket;
	int rec_len;
	char board[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
	int player = (number_of_games % 2 == 1) ? 1 : 2;
	int choice = 0;

	while (!check_win(board)){
		draw_board(board);

		player = (player % 2) ? 1 : 2;

		if (player == player_id){
			printf("\n\tPlayer %s, enter number: ", my_name);
			scanf("%d", &choice);
			while (!check_move(board, choice)){
				printf("\n\tPlayer %s, enter correct number: ", my_name);
				scanf("%d", &choice);
			}
			send(data_socket, &choice, sizeof(choice), 0);
		} else {
			printf("\n\tWaiting for %s move", opp_name);
			rec_len = recv(data_socket, &choice, MAX_RCV_LEN, 0);
			printf("\n\tPlayer %s move is: %d", opp_name, choice);
		}
		board[choice - 1] = (player == 1) ? 'X' : 'O';
		player++;
	} 

	draw_board(board);
	/*game is done, player won*/
	if (check_win(board) == 1){ 
		if (--player == player_id){
			res[MINE]++;
			printf("\n\t%s you are winner\n%s your score is: %d\nyour opponent %s score is: %d",
							my_name, my_name, res[MINE], opp_name, res[OPP]);
		} else {
			res[OPP]++;
			printf("\n\t%s is winner\n\t%s your score is: %d\n\tyour opponent %s score is: %d",
						opp_name, my_name, res[MINE], opp_name, res[OPP]);
		}
	} else {
		printf("\n\tDRAW\n%s your score is: %d\nyour opponent %s score is: %d",
				my_name, res[MINE], opp_name, res[OPP]);
	}

	/*play one more game*/
	printf("\n\tDo you wanna play one more round? (y/n) ");
	fgetc(stdin);
	fgets(buf, sizeof buf, stdin);
	buf[strlen(buf) - 1] = '\0';
	printf("\n\tWating for %s response\n", opp_name);

	if (strcmp(buf, "y") != 0) {
		printf("\n\t%s you don't want to play\n", my_name);
		res[MINE] = 0;
		res[OPP] = 0;
		return 1;
	}

	send(data_socket, buf, strlen(buf), 0);
	rec_len = recv(data_socket, buf, MAX_RCV_LEN, 0);
	buf[rec_len] = '\0';
	if (strcmp(buf, "y") != 0) {
		printf("\n\t%s doesn't want to play\n", opp_name);
		res[MINE] = 0;
		res[OPP] = 0;
		return 2;
	}
	return 0;
}

void draw_board(char *board)
{
	printf(" \n\t ___ ___ ___\n");
	printf("\t|   |   |   |\n");
	printf("\t| %c | %c | %c |\n", board[0], board[1], board[2]);
	printf("\t|___|___|___|\n");
	printf("\t|   |   |   |\n");
	printf("\t| %c | %c | %c |\n", board[3], board[4], board[5]);
	printf("\t|___|___|___|\n");
	printf("\t|   |   |   |\n");
	printf("\t| %c | %c | %c |\n", board[6], board[7], board[8]);
	printf("\t|___|___|___|\n");
}

/*return 1 if there is a winner, game is done
return 2 if there is no winner, game is done
return 0 if game is in progress*/
unsigned short check_win(char *board)
{
	for (int i = 0; i < 9; i += 3) {
		if (board[i] == board[i + 1] && board[i] == board[i + 2])
			return 1;
	}

	for (int i = 0; i < 3; i++) {
		if (board[i] == board[i + 3] && board[i] == board[i + 6])
			return 1;
	}

	if (board[0] == board[4] && board[0] == board[8] || board[2] == board[4] && board[2] == board[6])
		return 1;

	for (int i = 0; i < 9; i++){
		if (board[i] == i + '1')
			return 0;
	}
	return 2;
}

/*return 1 if move is ok
return 0 if move is not ok*/
unsigned short check_move(char *board, int choice)
{
	if (choice < 1 || choice > 9)
		return 0;
	else if (board[choice - 1] != choice + '0')
		return 0;
	else
		return 1;
}
