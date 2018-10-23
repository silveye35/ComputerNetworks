#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>


/*dispBoard
*prints board by inserting spaces
*/
static void dispBoard(char* board, uint8_t len);
/*------------------------------------------------------------------------
* Program: prog2_client
*
* Purpose: allocate a socket, connect to a server,
*          and allow user to play word game
*
* Syntax: ./prog2_client server_address server_port
*
* server_address - name of a computer on which server is executing
* server_port    - protocol port number server is using
*
*------------------------------------------------------------------------
*/
int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */



	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) /* test for legal value */
	sad.sin_port = htons((u_short)port);
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	if (connect(sd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

	/* Game starts here */
	char board[255];
	char userGuess[255];
	uint8_t maxTime = 0;
	uint8_t valid = 1;
	uint8_t guessSize = 0;
	uint8_t boardSize = 0;
	uint8_t roundNum = 0;
	uint8_t score = 0;
	uint8_t oppScore = 0;
	uint8_t timeout = 0;
	uint8_t playerNum = 0;
	char goCheck = 'Y';
	int blockFlag = fcntl(sd, F_GETFL);
	int nonBlockFlag = (blockFlag | O_NONBLOCK);

	//recv player #
	recv(sd, &playerNum, sizeof(playerNum), 0);//recv playerNum
	recv(sd, &boardSize, sizeof(boardSize), 0);//recv init board size
	recv(sd, &maxTime, sizeof(maxTime), 0);//recv time

	printf("You are player number %d\n", playerNum);
	printf("Board size: %d\n", boardSize);
	printf("Time per guess: %d seconds\n", maxTime);

	while ((score < 3) && (oppScore < 3)) {

		recv(sd, &score, sizeof(score), 0);//recv score
		recv(sd, &oppScore, sizeof(oppScore), 0);//recv iopponent score
		if((score < 3) && (oppScore < 3)){//checks if game is over

			recv(sd, &roundNum, sizeof(roundNum), 0);//recv round
			valid = 1;
			recv(sd, &boardSize, sizeof(boardSize), 0);//recv actual board size
			recv(sd, board, boardSize, 0); //recieves board
			board[boardSize] = '\0';

			printf("\nRound number %d\n", roundNum);
			printf("Score: %d - %d\n", score, oppScore);
			dispBoard(board, boardSize);

			while(valid != 0){
				recv(sd, &goCheck, sizeof(goCheck), 0);//recv goCheck
				if(goCheck == 'Y'){
					printf("%s\n","What is your guess?: ");
					fgets(userGuess, 100, stdin);
					guessSize = strlen(userGuess)-1;

					fcntl(sd, F_SETFL, nonBlockFlag);
					recv(sd, &timeout, sizeof(timeout), 0);//recv timeout (nonblocking)
					fcntl(sd, F_SETFL, blockFlag);
					if(timeout != 0){
						printf("you timed out\n");
						timeout = 0;
					}else{
						send(sd, &guessSize, sizeof(guessSize), 0);//send userGuess2
						send(sd, userGuess, guessSize, 0);//send userGuess2
					}

					recv(sd, &valid, sizeof(valid), 0);//recv valid
					if(valid != 0){
						printf("Valid!\n");
					}else{
						printf("Invalid!\n");
					}
				}
				else{
					printf("Please wait for your opponent to enter a word\n");
					recv(sd, &valid, sizeof(valid), 0);//recv valid
					if(valid != 0){
						recv(sd, userGuess, valid, 0);
						userGuess[valid] = '\0';
						printf("Opponent answered: %s\n", userGuess);
					}else{
						printf("Opponent entered an invalid word\n");
					}
				}
			}
		}
	}//end of game loop


	if(score == 3){
	 	printf("\n%s\n", "You win!");
	}else{
		printf("\n%s\n", "You lost.");
	}

	close(sd);
	exit(EXIT_SUCCESS);
}

/*dispBoard
*char* board: string representing the board
*uint8_t len: length of the board
*/
static void dispBoard(char* board, uint8_t len){
	for(int i = 0; i<len; i++){
		printf("%c ", board[i]);
	}
	printf("\n");
}
