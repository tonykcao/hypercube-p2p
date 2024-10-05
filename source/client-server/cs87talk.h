#ifndef _CS87TALK_H_
#define _CS87TALK_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


// ***************** START: DO NOT CHANGE THESE DEFINITOINS
//
// The cs87talk server listen port number:
#define CS87_TALKSERVER_PORTNUM	1888

// Message Tag definitions:
#define HELLO_MSG               1
#define MSG_DATA                2
#define QUIT                    3
#define HELLO_OK                4
#define HELLO_ERROR             5
#define MAX_NAME_LENGTH         255

//max number of clients accepted simultaneously
#define MAX_CLIENTS  256

// max size of a message associated with MSG_DATA
#define MSGMAX    255   

// max name length associated with HELLO_MSG.
#define NAMEMAX   16 

#define printflush(s, ...) do { printf(s, ## __VA_ARGS__); fflush(stdout); } while (0)

// type to store/send/receive message tags and message sizes
// use tsize_t to declare variables storing these values:
//   tsize_t  len, tag;
// NOTE: can store up to 255 (related to MSGMAX size)
typedef unsigned char tsize_t; 
// ***************** END: DO NOT CHANGE THESE DEFINITIONS

// Add any other definitions, function prototypes, etc. here:

int send_msg(int sockfd, tsize_t *buf, tsize_t len);
int recv_msg(int sockfd, tsize_t *buf, tsize_t len);
void t_cast(char *msg, tsize_t *t_msg, int max);
int MSGSend(int confd, tsize_t *tag, tsize_t *len, tsize_t *msg);
int MSGReceive(int confd, tsize_t *tag, tsize_t *len, tsize_t *msg);

#endif
