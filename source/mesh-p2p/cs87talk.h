#ifndef _CS87TALK_H_
#define _CS87TALK_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// The cs87talk server listen port number:
#define CS87_TALKSERVER_PORTNUM 1888

// Message Tag definitions:
#define HELLO_MSG 1
#define MSG_DATA 2
#define QUIT 3
#define HELLO_OK 4
#define HELLO_ERROR 5
#define MAX_NAME_LENGTH 255

// max number of clients accepted simultaneously
#define MAX_CLIENTS 256

// max size of a message associated with MSG_DATA
#define MSGMAX 255

// max name length associated with HELLO_MSG.
#define NAMEMAX 16

#define BACKLOG 10

#define printflush(s, ...) do { printf(s, ## __VA_ARGS__); fflush(stdout); } while (0)

#define MSGLOGMAX 128

#define MAX_LOG_LENGTH 512

// type to store/send/receive message tags and message sizes
// use tsize_t to declare variables storing these values:
//   tsize_t  len, tag;
// NOTE: can store up to 255 (related to MSGMAX size)
typedef unsigned char tsize_t;

struct Client {
    int socket_fd;
    tsize_t name_len;
    tsize_t name[NAMEMAX];
    tsize_t ip_len;
    tsize_t ip[MSGMAX];
    tsize_t port_len;
    tsize_t port[MSGMAX];
};

/**
 * The basic level function for sending data through socket.
 * If a send is needed, use this function.
 * @param sockfd socket file descriptor
 * @param buf buffer to send
 * @param len length of the msg
 * @return number of bytes sent, -1 if error, 0 if connection closed
 */
int socketSend(int sockfd, tsize_t *buf, tsize_t len);

/**
 * The basic level function for receiving data through socket.
 * If a receive is needed, use this function.
 * @param sockfd socket file descriptor
 * @param buf buffer to receive
 * @param len length of the msg
 * @return number of bytes received, -1 if error, 0 if connection closed
 */
int socketRecv(int sockfd, tsize_t *buf, tsize_t len);

/**
 * Wrapper function for sending an actual message in the chat.
 * @param sockfd socket file descriptor
 * @param tag the tag of the message
 * @param len the length of the message
 * @param msg the message
 * @return 1 if success, 0 if connection closed, -1 if error
 */
int MSGSend(int sockfd, tsize_t *tag, tsize_t *len, tsize_t *msg, tsize_t *ID_len, tsize_t *ID);

/**
 * Wrapper function for receiving an actual message in the chat.
 * @param sockfd socket file descriptor
 * @param tag the tag of the message
 * @param len the length of the message
 * @param msg the message
 * @return 1 if success, 0 if connection closed, -1 if error
 */
int MSGReceive(int sockfd, tsize_t *tag, tsize_t *len, tsize_t *msg, tsize_t *ID_len, tsize_t *ID);

/**
 * Helper function to cast a char * to tsize_t *.
 * @param msg the char * to cast
 * @param t_msg the buffer to store the casted msg
 * @param max the max length of the msg
 */
void tCast(char *msg, tsize_t *t_msg, size_t max);

/**
 * Check if the socket connection has error (ret < 0).
 * @param ret return value of the socket function
 * @param sockfd socket file descriptor
 * @param msg the error message to print
 */
void socketErrorCheck(int ret, int sockfd, char *msg);

/**
 * Check if the socket connection is closed (ret == 0).
 * @param ret return value of the socket function
 * @param sockfd socket file descriptor
 * @param msg the error message to print
 */
void socketExitCheck(int ret, int sockfd, char *msg);

/**
 * Check if the socket connection has error (ret < 0). Calls thread exit if failed.
 * @param ret return value of the socket function
 * @param sockfd socket file descriptor
 * @param msg the error message to print
 */
void t_socketErrorCheck(int ret, int sockfd, char *msg);

/**
 * Check if the socket connection is closed (ret == 0). Calls thread exit if failed.
 * @param ret return value of the socket function
 * @param sockfd socket file descriptor
 * @param msg the error message to print
 */
void t_socketExitCheck(int ret, int sockfd, char *msg);

/**
 * Function to get the IP address of the current machine.
 * @return the IP address of the current machine
 */
char *getIP();

int _atoi(tsize_t* str);

#endif
