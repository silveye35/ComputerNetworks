#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include "trie.h"

#define QLEN 6 /* size of request queue */

/*vowelCheck
*checks if a given char is a vowel(a,e,i,o,u)
*returns 1 if the char is a vowel, 0 if not.
*/
static int vowelCheck(int newChar);

/*checkBoard
*checks if a given string(userGuess) includes only characters from
*another string(board)
*returns 1 if guess is legal, 0 if not.
*/
static int checkBoard(char* board, char* userGuess);


/*main
*Runs word guessing game
*Accepts user input as guesses for strings in using characters from the board.
*Game runs until one player wins three rounds.
*
* Syntax: ./prog2_server server_port board_size timeout dictionary
* server_port    - protocol port number server is using
* board_size     - minimum size of the game board
* timeout        - time in seconds each player has to provide a guess
* dictionary     - path to filewith dictionary data
*/
int main(int argc, char** argv){
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold server's address */
  struct sockaddr_in cad; /* structure to hold client's address */
  int sd, sd2, sd3; /* socket descriptors */
  socklen_t alen; /* length of address */
  int optval = 1; /* boolean value when we set socket option */
  int port; /* protocol port number */

  if(argc != 5){
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


    alen = sizeof(cad);

    if ( (sd2=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
      fprintf(stderr, "Error: Accept failed\n");
      exit(EXIT_FAILURE);
    }

    if ( (sd3=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
      fprintf(stderr, "Error: Accept failed\n");
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Both connections Accepted\n");

    cpid = fork();
    if(cpid == 0){

      close(sd);

      /*  @@@@@                INITIALIZES GAME         @@@@@@@      */
      char board[255];
      char newChar;
      int vowelFound = 0;
      uint8_t wordLength = atoi(argv[2]);
      uint8_t roundTime = atoi(argv[3]);
      char* dictionary = argv[4];
      uint8_t k;
      uint8_t score1 = 0;
      uint8_t score2 = 0;
      int roundLost = 0;
      int seed = time(NULL);
      char userGuess[255];
      char word[50];
      int error = 0;
      uint8_t roundNum = 1;
      uint8_t timeout = 0;
      uint8_t valid = 0;
      uint8_t guessSize = 0;
      uint8_t playerNum = 1;
      char go = 'Y';
      char no = 'N';
      int playerTurn = 1;

      struct TrieNode *root = getNode(); //dictionary trie struct
      struct TrieNode *guessTrie; //guesses trie struct

      struct timeval tv;
      tv.tv_sec = roundTime;
      tv.tv_usec = 0;
      setsockopt(sd2, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
      setsockopt(sd3, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

      //parse file into trie
      FILE *dict;
      if((dict = fopen(dictionary, "r")) == NULL){
        printf("%s\n", "Dictionary file missing.");
        exit(1);
      }

      while(fscanf(dict, "%s",word) != EOF){
        insert(root, word);
      }

      //send player #
      error = send(sd2, &playerNum, sizeof(playerNum), 0); //send player num
      error = send(sd2, &wordLength, sizeof(wordLength), 0);//send init board size
      error = send(sd2, &roundTime, sizeof(roundTime), 0);//send time
      playerNum = 2;
      error = send(sd3, &playerNum, sizeof(playerNum), 0); //send player num
      error = send(sd3, &wordLength, sizeof(wordLength), 0);//send init board size
      error = send(sd3, &roundTime, sizeof(roundTime), 0);//send time



      //GAME LOOP
      while((score1 < 3) && (score2 < 3)){
        //init trie and random num generator
        seed = time(NULL);
        srand(seed);
        guessTrie = getNode();


        //setup board
        vowelFound = 0;
        for(k=0; k<wordLength; k++){
          newChar= 97 + (rand()%26);
          board[k] = newChar;
          if(vowelCheck(newChar)){
            vowelFound = 1;
          }
        }
        while(!vowelFound){
          newChar = 97 + (rand()%26);
          board[k] = newChar;
          if(vowelCheck(newChar)){
            vowelFound = 1;
          }
          k++;
        }
        board[k] = '\0';

        error = send(sd2, &score1, sizeof(score1), 0);//send score
        error = send(sd2, &score2, sizeof(score2), 0);//send score
        error = send(sd2, &roundNum, sizeof(roundNum), 0);//send round
        error = send(sd2, &k, sizeof(k), 0);//send actual board size
        error = send(sd2, board, k, 0);//send board

        error = send(sd3, &score2, sizeof(score2), 0);//send score
        error = send(sd3, &score1, sizeof(score1), 0);//send score
        error = send(sd3, &roundNum, sizeof(roundNum), 0);//send round
        error = send(sd3, &k, sizeof(k), 0);//send actual board size
        error = send(sd3, board, k, 0);//send board

        roundLost = 0;
        if((roundNum%2) == 0){
          playerTurn = 2;
        }
        else{
          playerTurn = 1;
        }
        //ROUND LOOP
        while(!roundLost){
          //PLAYER 1
          if(playerTurn == 1){
            memset(userGuess, 0, sizeof(userGuess));
            memset(&guessSize, 0, sizeof(guessSize));

            error = send(sd2, &go, sizeof(go), 0);//sends char
            error = send(sd3, &no, sizeof(no), 0);//sends char
            error = recv(sd2, &guessSize, sizeof(guessSize), 0);//recv guess size

            if((error == -1) && (errno == EAGAIN)){
              guessSize = 0;
              userGuess[0] = '\0';
              timeout = 1;
              error = send(sd2, &timeout, sizeof(timeout), 0);//sends timeout
            }else if(error == 0){
              roundLost = 1;
              score2 = 2;
              guessSize = 0;
              userGuess[0] = '\0';
            }else{
              error = recv(sd2, userGuess, guessSize, 0);//recv guess
              userGuess[guessSize] = '\0';
            }

            //checks validity of guess
            if(search(guessTrie, userGuess)){
              roundLost = 1;
              score2++;
              valid = 0;
            }else if(!checkBoard(board, userGuess)){
              roundLost = 1;
              score2++;
              valid = 0;
            }else if(!search(root, userGuess)){
              roundLost = 1;
              score2++;
              valid = 0;
            }
            else{
              insert(guessTrie, userGuess);
              valid = guessSize;
            }
            error = send(sd2, &valid, sizeof(valid), 0);//sends validity
            error = send(sd3, &valid, sizeof(valid), 0);//sends validity
            if(valid != 0){
              error = send(sd3, userGuess, guessSize, 0);//sends guess to opponent
            }
            playerTurn = 2;

          }else{

            //PLAYER 2
            memset(userGuess, 0, sizeof(userGuess));
            memset(&guessSize, 0, sizeof(guessSize));

            error = send(sd2, &no, sizeof(no), 0);//sends char
            error = send(sd3, &go, sizeof(go), 0);//sends char

            error = recv(sd3, &guessSize, sizeof(guessSize), 0);//recv guess size

            if((error == -1) && (errno == EAGAIN)){
              guessSize = 0;
              userGuess[0] = '\0';
              timeout = 1;
              error = send(sd3, &timeout, sizeof(timeout), 0);//sends timeout
            }else if(error == 0){
              roundLost = 1;
              score1 = 2;
              guessSize = 0;
              userGuess[0] = '\0';
            }else{
              error = recv(sd3, userGuess, guessSize, 0);//recv guess
              //printf("%s %d\n", "error(2): ", error);
              userGuess[guessSize] = '\0';
            }

            //checks validity of guess
            if(search(guessTrie, userGuess)){
              roundLost = 1;
              score1++;
              valid = 0;
            }else if(!checkBoard(board, userGuess)){
              roundLost = 1;
              score1++;
              valid = 0;
            }else if(!search(root, userGuess)){
              roundLost = 1;
              score1++;
              valid = 0;
            }
            else{
              insert(guessTrie, userGuess);
              valid = guessSize;
            }
            error = send(sd3, &valid, sizeof(valid), 0);//sends validity
            error = send(sd2, &valid, sizeof(valid), 0);//sends validity
            if(valid != 0){
              error = send(sd2, userGuess, guessSize, 0);//sends guess to opponent
            }
            playerTurn = 1;

          }
        } //END ROUND LOOP
        roundNum++;
      }//END GAME LOOP
      error = send(sd2, &score1, sizeof(score1), 0);//send score
      error = send(sd2, &score2, sizeof(score2), 0);//send score
      error = send(sd3, &score2, sizeof(score2), 0);//send score
      error = send(sd3, &score1, sizeof(score1), 0);//send score
      close(sd2); //closes sd2
      close(sd3);
      exit(EXIT_SUCCESS);
    }
    close(sd2);
    close(sd3);
  }
}

/*checkBoard
*char* board- array of chars representing the board
*char* userGuess- array of chars representing the user's guess
*return value- int representing whether userGuess contains only characters
* from board. ! if true, 0 if false.
*/
static int checkBoard(char* board, char* userGuess){
  char boardCpy[strlen(board)+1];
  char* charLoc;
  int len = strlen(userGuess);
  strcpy(boardCpy, board);

  for(int i = 0; i<len; i++){
    charLoc = strchr(boardCpy, userGuess[i]);
    if(charLoc!= NULL){
      charLoc[0] = '$';
    }
    else{
      return 0;
    }
  }
  return 1;
}


/*vowelCheck
*int newChar- int representing the ascii value of a character
*return value: int representing whether the character is a vowel.
* 1 if true, 0 if false.
*/
static int vowelCheck(int newChar){
  int returnVal = 0;
  switch(newChar){
    case 97:
    case 101:
    case 105:
    case 111:
    case 117: returnVal = 1;
    break;
    default: returnVal = 0;
    break;
  }
  return returnVal;
}
