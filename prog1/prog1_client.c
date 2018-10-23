/* demo_client.c - code for example client program that uses TCP */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*dispSecret
*displays currently revealed  letters in secret word, and number of remaining
*guesses.
*/
void dispSecret(char* dispString, int guesses);

/*wincheck
 *checks if user wins or loses when the game ends
 */
int winCheck(char* dispString);

/*------------------------------------------------------------------------
* Program: prog1_client
*
* Purpose: allocate a socket, connect to a server, and print all output
*
* Syntax: ./demo_client server_address server_port
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
	char buf[1000]; /* buffer for data from the server */
	char buf2[1000];

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

	/* Repeatedly read data from socket and write to user's screen. */

	char* dispString;
	char userInput[2];
	char* midpoint;
	int guesses=1;
	while (guesses > 0) {
		recv(sd, buf, sizeof(buf), 0); //recieves board
		recv(sd, buf2, sizeof(buf2), 0); //recieves guesses

		dispString = buf;

		midpoint = buf2;
		guesses = atoi(midpoint);

		if(guesses>0){
			dispSecret(dispString, guesses);
		}

		if(guesses>0){
			userInput[0] = getchar();
			gets(midpoint);

			send(sd, userInput, sizeof(userInput), 0);
		}
	}

	printf("\n%s %s\n", "Board: ", dispString);
	if(!winCheck(dispString)){
		printf("%s\n", "You lost");
	}else{
		printf("%s\n", "You win.");
	}

	close(sd);
	exit(EXIT_SUCCESS);

}

/*
*dispString is the string sent from the server.
*if it contains any '-' characters, the user loses.
*returns 1 if the user wins,
* 0 if the user loses
*/
int winCheck(char* dispString){
	int i = 0;
	while(dispString[i] != '\0'){
		if(dispString[i] == '-'){
			return 0;
		}
		i++;
	}
	return 1;
}
/*
*dispString is the string sent from the server
*guesses is the remaining number of guesses
*/
void dispSecret(char* dispString, int guesses){
	printf("\n%s %s %s %d %s", "Board: ", dispString, "(", guesses, "guesses left)\nEnter guess: ");
}
