#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

/*------------------------------------------------------------------------
* Program: prog3_server.c
*
* Purpose: allocate a socket, connect to a server,
*          and allow user to participate in chat room
*
* Syntax: ./prog3_server participant_port observer_port
*
* participant_port - protocol port number for participants to connect to
* observer_port    - protocol port number for observers to connect to
*
*------------------------------------------------------------------------
*/

#define QLEN 6 /* size of request queue */
//message flags
#define MAXTIME 30
#define BROADCAST 1
#define WHISPER 2
#define PARTCON 3
#define PARTDISC 4
#define OBSCON 5
#define OBSDISC 6

//node in linked list
struct Users{
  char* user;
  int socketObs;
  int socketPar;
  int nameChosen;
  struct Users *next;
};

int checkNameChosen(struct Users* head, int sd);
int setNameChosen(struct Users* head, int sd, char* name);
static void updateClients(fd_set users, char string[1000], int i, int sd, int sockObs);
static void buildMessage(char* message, char* user, uint8_t messageType, fd_set users, int i, int sd, int sockObs);
static uint8_t valUserName(char* username);
static void deleteNode(struct Users **head_ref, int socketPar);
static void whisper(struct Users* head_ref, char* string, char* userSending);
static void append(struct Users** head_ref, char* user, int socketObs, int socketPar);
static char* searchSock(struct Users* head, int sd);
static int insertObs(struct Users* head, char* username, int sd);
static int deleteObs(struct Users* head, int sd);
static int searchObs(struct Users* head, char* username);
static int searchUser(struct Users* head, char* username);
int checkIfPart(struct Users* head, int sd);

int main(int argc, char** argv){
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold server's address */
  struct sockaddr_in cad; /* structure to hold client's address */
  struct protoent *ptrpObs; /* pointer to a protocol table entry */
  struct sockaddr_in sadObs; /* structure to hold server's address */
  struct sockaddr_in cadObs; /* structure to hold client's address */
  int sd, sd2, client_socket[255], sockObs; /* socket descriptors */
  socklen_t alen; /* length of address */
  int optval = 1; /* boolean value when we set socket option */
  int port, portObs; /* protocol port number */
  fd_set readfds;

  char* obs[255];
  char* partics[255];
  int numParts = 0;
  int numObs = 0;

  time_t partStart[255];
  time_t partEnd[255];
  time_t obsStart[255];
  time_t obsEnd[255];

  for(int i = 0; i < 255; i++){
    partStart[i] = 0;
    obsStart[i] = 0;
  }

  if(argc != 3){
    fprintf(stderr,"Error: Wrong number of arguments\n");
    exit(EXIT_FAILURE);
  }

  /*            @@@@@@@@           INITIALIZES SERVER            @@@@@@@@   */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;
  sad.sin_addr.s_addr = INADDR_ANY;
  port = atoi(argv[1]); /* protocol port number */
  portObs = atoi(argv[2]);

  if (port > 0) { /* test for illegal value */
    //TODO: set port number. The data type is u_short
    sad.sin_port = htons(port);
  } else { /* print error message and exit */
    fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
    exit(EXIT_FAILURE);
  }
  if (portObs > 0) { /* test for illegal value */
    //TODO: set port number. The data type is u_short
    sadObs.sin_port = htons(portObs);
  } else { /* print error message and exit */
    fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
    exit(EXIT_FAILURE);
  }

  /* Map TCP transport protocol name to protocol number */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }
  /* Map TCP transport protocol name to protocol number */
  if ( ((long int)(ptrpObs = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }
  sockObs = socket(AF_INET, SOCK_STREAM, ptrpObs->p_proto);
  if (sockObs < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  /* Allow reuse of port - avoid "Bind failed" issues */
  if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
    fprintf(stderr, "Error Setting socket option failed\n");
    exit(EXIT_FAILURE);
  }
  /* Allow reuse of port - avoid "Bind failed" issues */
  if( setsockopt(sockObs, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
    fprintf(stderr, "Error Setting socket option failed\n");
    exit(EXIT_FAILURE);
  }
  //binds participant to server
  if (bind(sd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
    fprintf(stderr,"Error: Bind failed\n");
    exit(EXIT_FAILURE);
  }
  //binds observer to server
  if (bind(sockObs, (struct sockaddr*)&sadObs, sizeof(sadObs)) < 0) {
    fprintf(stderr,"Error: Bind failed\n");
    exit(EXIT_FAILURE);
  }
  //listens for participants
  if (listen(sd, QLEN) < 0) {
    fprintf(stderr,"Error: Listen failed\n");
    exit(EXIT_FAILURE);
  }
  //listens to observers
  if (listen(sockObs, QLEN) < 0) {
    fprintf(stderr,"Error: Listen failed\n");
    exit(EXIT_FAILURE);
  }

  //chat room code starts here

  uint8_t data = 1;
  int error = 0;
  uint16_t guessSize = 0;
  char userGuess[1000];
  fd_set users;
  fd_set usersCopy;
  uint8_t valid = 0;
  struct Users* head = NULL;
  char* outputString;
  int outputLen;
  int hangupSock = 0;
  uint8_t flagType = 0;/*1=broadcast message; 2=whisper;3=participant connect;4=participant dc'd;5=observer joined */
  char go = 'Y';
  char no = 'N';

  alen = sizeof(cad);

  FD_ZERO(&users);
  FD_SET(sd, &users);
  FD_SET(sockObs, &users);
  while(1){

    memset(userGuess, 0, sizeof(userGuess));
    usersCopy = users;
    if (select(FD_SETSIZE, &usersCopy, NULL, NULL, NULL) < 0){
      perror ("select");
      exit (EXIT_FAILURE);
    }
    //check times
    for(int i = 0; i < 255; i++){
      if(partStart[i] != 0){
        time(&partEnd[i]);
        if(partEnd[i]-partStart[i] >= MAXTIME){
          deleteNode(&head, i);
          FD_CLR(i, &users);
          close(i);
          partStart[i] = 0;
        }
      }
      if(obsStart[i] != 0){
        time(&obsEnd[i]);
        if(obsEnd[i]-obsStart[i] >= MAXTIME){
          deleteNode(&head, i);
          FD_CLR(i, &users);
          close(i);
          obsStart[i] = 0;
        }
      }
    }

    for(int i = 0; i< FD_SETSIZE; i++){
      if(FD_ISSET(i, &usersCopy)){

        //new participant socket
        if(i == sd){
          if ( (sd2=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
            fprintf(stderr, "Error: Accept failed\n");
            exit(EXIT_FAILURE);
          }
          if(numParts >= 255){
            error = send(sd2, &no, sizeof(no), 0);//sends char
            close(sd2);
          }
          else{
            error = send(sd2, &go, sizeof(go), 0);//send Y
            append(&head, "", -1, sd2);
            FD_SET(sd2, &users);
            time(&partStart[sd2]);
          }
        }

        //new observer socket
        else if(i == sockObs){
          if ( (sd2=accept(sockObs, (struct sockaddr *)&cadObs, &alen)) < 0) {
            fprintf(stderr, "Error: Accept failed\n");
            exit(EXIT_FAILURE);
          }
          if(numObs >= 255){
            error = send(sd2, &no, sizeof(go), 0);//sends char
            close(sd2);
          }
          else{
            error = send(sd2, &go, sizeof(go), 0);//sends char
            FD_SET(sd2, &users);
            time(&obsStart[sd2]);
          }
        }

        //send message
        else{
          error = recv(i, &guessSize, sizeof(guessSize), 0);

          if(error == 0){//hangup user
            outputString = searchSock(head, i);

            if(outputString[0] == '\0'){//disconnected user was observer
              buildMessage(NULL, NULL, OBSDISC, users, i, sd, sockObs);
              numObs--;
              deleteObs(head, i);
            }else{//disconnected user was participant
              buildMessage(NULL, outputString, PARTDISC, users, i, sd, sockObs);
              hangupSock = (searchObs(head, outputString));
              if(hangupSock != -1){
                FD_CLR(hangupSock, &users);
                close(hangupSock);
              }
              deleteNode(&head, i);
              numParts--;
            }
            FD_CLR(i, &users);
            close(i);

          }else{//message recieved
            error = recv(i, userGuess, guessSize, 0);

            if(checkNameChosen(head, i)==0){//participant choose name
              userGuess[guessSize] = '\0';
              valid = valUserName(userGuess);
              if(searchUser(head, userGuess) != -1){
                valid = 0;
              }
              if(valid == 0){
                error = send(sd2, &no, sizeof(no), 0);//sends N
              }else{
                error = send(sd2, &go, sizeof(go), 0);//send Y
                numParts++;
                buildMessage(NULL, userGuess, PARTCON, users, i, sd, sockObs);
                setNameChosen(head, i, strdup(userGuess));
                partStart[i] = 0;
              }

            }else if(!checkIfPart(head, i)){//observer choose person
              userGuess[guessSize] = '\0';
              valid = insertObs(head, userGuess, sd2);
              if(valid == 0){
                error = send(sd2, &no, sizeof(no), 0);//sends N
              }else{
                error = send(sd2, &go, sizeof(go), 0);//send Y
                buildMessage(NULL, NULL, OBSCON, users, i, sd, sockObs);
                numObs++;
                obsStart[i] = 0;
              }

            }else{//send message
              outputString = searchSock(head, i);

              if(userGuess[0] == '@'){//whisper
                whisper(head, userGuess, outputString);
              }else{//broadcast
                buildMessage(userGuess, outputString, BROADCAST, users, i, sd, sockObs);
              }
            }
          }
        }
      }
    }
  }
}

//checks validity of username
//username: string representing the user's requested name
static uint8_t valUserName(char* username){
  int len = strlen(username);
  printf("User name: %s\n", username);
  if(!(len >= 1 && len <= 10)){
    return 0;
  }
  for(int i = 0;i < len; i ++){ //checks for valid chars
    if(!( (username[i] >= 65 && username[i] <= 90) ||
    (username[i] >= 97 && username[i] <= 122) ||
    (username[i] == 95) || (username[i] >= 48 && username[i] <= 57) )){
      return 0;
    }
  }
  return 1;
}

//broadcasts string to all users
//users: set of file descriptors containing the socket descriptors of all recipients
//size: size of string to send
//string: message to send
//i: socket descriptor of user sending message
//sd: participant connection socket
//sockObs: observer connection socket
static void updateClients(fd_set users, char string[1000], int i, int sd, int sockObs){
  uint16_t size = strlen(string);
  for(int j = 0; j< FD_SETSIZE; j++){
    if((FD_ISSET(j, &users)) && (j != i) && (j != sd) && (j != sockObs)){
      if(send(j, &size, sizeof(size), 0) < 0){
        fprintf(stderr,"send failed %d\n", j);
        exit(EXIT_FAILURE);
      }
      if(send(j, string, size, 0) < 0){
        fprintf(stderr,"send2 failed %d\n", j);
        exit(EXIT_FAILURE);
      }
    }
  }
}

//builds all messages broadcast to all sds
static void buildMessage(char* message, char* user, uint8_t messageType, fd_set users, int i, int sd, int sockObs){
  char returnString[1000];
  memset(returnString, 0, sizeof(char)*1000);
  int index = 0;
  int userSize;
  int messageSize;

  if(user != NULL){
    userSize = strlen(user);
  }
  if(message != NULL){
    messageSize = strlen(message);
  }

  switch(messageType){

    case BROADCAST:
    sprintf(returnString, ">%11s: %s", user, message);
    updateClients( users, returnString, i, sd, sockObs);
    break;

    case PARTCON:
    sprintf(returnString, "User %s has joined", user);
    updateClients( users, returnString, i, sd, sockObs);
    break;

    case PARTDISC:
    sprintf(returnString, "User %s has left", user);
    updateClients( users, returnString, i, sd, sockObs);
    break;

    case OBSCON:
    sprintf(returnString, "A new observer has joined");
    updateClients( users, returnString, i, sd, sockObs);
    break;

    case OBSDISC:
    sprintf(returnString, "An observer has left");
    updateClients( users, returnString, i, sd, sockObs);
    break;

    default:
    break;
  }
}

//sends whisper to specific user
static void whisper(struct Users* head_ref, char* string, char* userSending){
  char userGuess[1000];
  char* message = strchr(string, 32);
  uint8_t flagType = WHISPER;
  if(message == NULL){
    return;
  }
  int sockPart = 0;
  int sockObs = 0;
  uint16_t size = 0;
  uint16_t nameSize = sizeof(userSending) -1;
  string++;
  message[0] = '\0';
  message++;

  sprintf(userGuess, "-%11s: %s", userSending, message);

  sockPart = searchUser(head_ref, string);
  sockObs = searchObs(head_ref, string);

  if(sockPart != -1){//participant
    size = strlen(userGuess);
    if(send(sockPart, &size, sizeof(size), 0) < 0){//send message
      fprintf(stderr,"send failed %d\n", sockPart);
      exit(EXIT_FAILURE);
    }
    if(send(sockPart, userGuess, size, 0) < 0){
      fprintf(stderr,"send2 failed %d\n", sockPart);
      exit(EXIT_FAILURE);
    }
  }

  if(sockObs != -1){//observer
    size = strlen(userGuess);
    if(send(sockObs, &size, sizeof(size), 0) < 0){//send message
      fprintf(stderr,"send failed %d\n", sockObs);
      exit(EXIT_FAILURE);
    }
    if(send(sockObs, userGuess, size, 0) < 0){
      fprintf(stderr,"send2 failed %d\n", sockObs);
      exit(EXIT_FAILURE);
    }
  }
}

//adds a user to a linked list given a socketObs, socketPar, and the username
static void append(struct Users** head_ref, char* user, int socketObs, int socketPar){
  struct Users* new_node = (struct Users*) malloc(sizeof(struct Users));
  struct Users *last = *head_ref;

  new_node->user = user;
  new_node->socketObs  = socketObs;
  new_node->socketPar = socketPar;
  new_node->nameChosen = 0;
  new_node->next = NULL;

  if (*head_ref == NULL)
  {
    *head_ref = new_node;
    return;
  }

  while (last->next != NULL)
  last = last->next;

  last->next = new_node;
  return;
}

//deletes a username from the linked list and hangs them up from the connection
static void deleteNode(struct Users **head_ref, int socketPar){
  struct Users* temp = *head_ref, *prev;
  if (temp != NULL && temp->socketPar == socketPar)
  {
    *head_ref = temp->next;
    free(temp);
    return;
  }

  while (temp != NULL && temp->socketPar != socketPar)
  {
    prev = temp;
    temp = temp->next;
  }
  if (temp == NULL) return;
  prev->next = temp->next;

  free(temp);
}

//search for an observer given a username
int searchObs(struct Users* head, char* username){
  struct Users* current = head;
  while (current != NULL){
    if (strcmp(username, current->user) == 0){
      return current->socketObs;
    }
    current = current->next;
  }
  return -1;
}

//search for a user given a username. Return the sd of it if its there;otherwise, return -1
int searchUser(struct Users* head, char* username){
  struct Users* current = head;
  while (current != NULL){
    if (strcmp(username, current->user) == 0){
      return current->socketPar;
    }
    current = current->next;
  }
  return -1;
}

//insert an observer into our linked list
int insertObs(struct Users* head, char* username, int sd){
  struct Users* current = head;
  while (current != NULL)
  {
    if (strcmp(username, current->user) == 0){
      if(current->socketObs != -1){
        return 0;
      }
      current->socketObs = sd;
      return 1;
    }
    current = current->next;
  }
  return 0;
}

int deleteObs(struct Users* head, int sd){
  struct Users* current = head;
  while (current != NULL)
  {
    if (current->socketObs == sd){
      current->socketObs = -1;
      return 1;
    }
    current = current->next;
  }
  return 0;
}

//checks the name chosen given a socket descriptor
int checkNameChosen(struct Users* head, int sd){
  struct Users* current = head;
  int returnVal = 1;
  while (current != NULL){
    if (current->socketPar == sd){
      returnVal = current->nameChosen;
    }
    current = current->next;
  }
  return returnVal;
}

//go through a linked list and check if a user is a participant
int checkIfPart(struct Users* head, int sd){
  struct Users* current = head;
  int returnVal = 0;
  while (current != NULL){
    if (current->socketPar == sd){
      returnVal = 1;
    }
    current = current->next;
  }
  return returnVal;
}

//sets a name to a designated socket descriptor
int setNameChosen(struct Users* head, int sd, char* name){
  struct Users* current = head;
  char* returnVal = "\0";
  while (current != NULL){
    if (current->socketPar == sd){
      current->user = name;
      current->nameChosen = 1;
      return 1;
    }
    current = current->next;
  }
  return 0;
}

//searches a socket descriptor in our linked list
char* searchSock(struct Users* head, int sd){
  struct Users* current = head;
  char* returnVal = "\0";
  while (current != NULL){
    if (current->socketPar == sd){
      returnVal = current->user;
    }
    current = current->next;
  }
  return returnVal;
}
