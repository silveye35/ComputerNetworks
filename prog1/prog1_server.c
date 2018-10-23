#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define QLEN 6 /* size of request queue */

/*isIn
*checks secret word for any instances of a given character.
*If found, updates dispString to show the given character and returns 1.
*If not found, returns 0
*/
int isIn(char* secret, char* dispString, char userInput, int len);

/*main
*Runs hangman game.
*Accepts user input as single characters as guesses for letters in secret word
*When user runs out of guesses or guesses complete word, game ends
*/
int main(int argc, char** argv){
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold server's address */
  struct sockaddr_in cad; /* structure to hold client's address */
  int sd, sd2; /* socket descriptors */
  socklen_t alen; /* length of address */
  int optval = 1; /* boolean value when we set socket option */
  int port; /* protocol port number */
  char buf[1000]; /* buffer for string the server sends */

  char* secret;
  int len;
  char* dispString;
  char guessString[3];
  int guesses;
  char userInput;

  if(argc != 3){
    fprintf(stderr,"Error: Wrong number of arguments\n");
    exit(EXIT_FAILURE);
  }


  /*            @@@@@@@@           INITIALIZES SERVER            @@@@@@@@   */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;
  sad.sin_addr.s_addr = INADDR_ANY;
  port = atoi(argv[1]); /* protocol port number */

  if (port > 0) { /* test for illegal value */
    //TODO: set port number. The data type is u_short
    sad.sin_port = htons(port);
  } else { /* print error message and exit */
    fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
    exit(EXIT_FAILURE);
  }

  /* Map TCP transport protocol name to protocol number */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  /* Allow reuse of port - avoid "Bind failed" issues */
  if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
    fprintf(stderr, "Error Setting socket option failed\n");
    exit(EXIT_FAILURE);
  }

  if (bind(sd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
    fprintf(stderr,"Error: Bind failed\n");
    exit(EXIT_FAILURE);
  }

  if (listen(sd, QLEN) < 0) {
    fprintf(stderr,"Error: Listen failed\n");
    exit(EXIT_FAILURE);
  }
  pid_t cpid;
  while(1){


    alen = sizeof(cad);            guesses = 0;

    if ( (sd2=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
      fprintf(stderr, "Error: Accept failed\n");
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Connection Accepted\n");

    cpid = fork();
    if(cpid == 0){

      close(sd);

      /*  @@@@@                INITIALIZES GAME         @@@@@@@      */

      secret = argv[2];
      len = strlen(secret);
      dispString  = malloc(len+1);
      guesses = len;
      for(int i = 0; i<len; i++){
        dispString[i] = '-';
      }
      dispString[len] = '\0';

      if(len < 10){
        guessString[0] = guesses+48;
        guessString[1] = '\0';

      }
      else{
        guessString[1] = (guesses%10)+48;
        guessString[0] = (guesses/10)+48;
        guessString[2] = '\0';
      }

      int error = 0;
      while(guesses > 0){

        if(len<10){
          guessString[0] = guesses+48;
        }
        else{
            guessString[1] = (guesses%10)+48;
            guessString[0] = (guesses/10)+48;
        }

        error = send(sd2, dispString, strlen(dispString), 0);//send board
	      error = send(sd2, guessString, strlen(guessString), 0);//send guesses
        error = recv(sd2, buf, sizeof(buf
        +36), 0); //receives user input
        if(error > 0){
          userInput = buf[0];
          if(!(isIn(secret, dispString, userInput, len))){
            guesses--;
          }

          if(strcmp(dispString, secret) == 0){
            guesses = 0;
          }

        }

      }
      dispString[len+1] = '0';
      if(len >= 10){ //if condition for if the user input is longer than ten characters
        dispString[len+2] = '0';
      }
      error = send(sd2, dispString, strlen(dispString), 0);
      close(sd2); //closes sd2
      exit(EXIT_SUCCESS);
    }
    close(sd2);
  }
}

/*
*secret is the word the user is trying to guess
*dispString is the currently revealed word to send to the user
*userInput is the character sent from the client
*len is the length of secret
*
*The return value is 1 if secret contains userInput, 0 if not
*/
int isIn(char* secret, char* dispString, char userInput, int len){
  int returnVal = 0;

  for(int i = 0; i < len; i ++){
    if(dispString[i] == userInput){ //if the user guesses correctly
      return 0;
    }
  }

  for(int i = 0; i < len; i ++){
    if(secret[i] == userInput){ //if the user guesses correctly
      dispString[i] = userInput;
      returnVal = 1;
    }
  }

  return returnVal;
}
