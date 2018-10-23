#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

/*------------------------------------------------------------------------
* Program: prog3_observer
*
* Purpose: allocate a socket, connect to a server,
*          and allow user to observe chat room
*
* Syntax: ./prog3_observer server_address server_port
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

	//connects observer to server
	if (connect(sd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

	uint8_t messageType = 0;
	char userData[1000];
	uint16_t userDataSize = 0;
	char serverData[1000];
	uint16_t serverDataSize = 0;
	char serverData2[1000];
	uint16_t serverData2Size = 0;
	fd_set inputs;
	fd_set inputsCopy;
	FD_ZERO(&inputs);
	FD_SET(sd, &inputs);
	uint8_t valid = 0;
	char check;
	time_t timeStart;
	time_t timeEnd;

	//number of observers check
	recv(sd, &check, sizeof(check), 0);
	if(check == 'N'){
		perror ("Server full");
		exit (EXIT_FAILURE);
	}

	//username entry
	while(valid == 0){
		printf("Enter a username:\n");
		time(&timeStart);
		fgets(userData, 254, stdin);
		time(&timeEnd);
		if(timeEnd-timeStart > 30){
			close(sd);
			perror ("timeout");
			exit (EXIT_FAILURE);
		}
		userDataSize = strlen(userData)-1;
		send(sd, &userDataSize, sizeof(userDataSize), 0);
		send(sd, userData, userDataSize, 0);
		recv(sd, &check, sizeof(check), 0);
		if(check == 'Y'){
			valid = 1;
		}
		if(valid == 0){
			printf("Invalid username\n");
		}
	}

	//chat client main loop
	while (1){
		inputsCopy = inputs;
		if (select(FD_SETSIZE, &inputsCopy, NULL, NULL, NULL) < 0){
			perror ("select");
			exit (EXIT_FAILURE);
		}
		for(int i = 0; i< FD_SETSIZE; i++){
      if(FD_ISSET(i, &inputsCopy)){
				if(recv(i, &serverDataSize, sizeof(serverDataSize), 0) == 0){
					close(sd);
					exit(EXIT_SUCCESS);
				}//recv name
				recv(i, serverData, serverDataSize, 0);
				serverData[serverDataSize] = '\0';
				printf("%s\n", serverData);
			}
		}
	}
	close(sd);
	exit(EXIT_SUCCESS);
}
