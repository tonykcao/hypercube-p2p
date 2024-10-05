#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include "cs87talk.h"

/**************************************************************/

int socketSend(int sockfd, tsize_t *buf, tsize_t len) {
    int ret;
    tsize_t curr = 0;

    while (curr < len) {
        ret = send(sockfd, &(buf[curr]), len, MSG_NOSIGNAL);
        if (ret == -1 || ret > len) {
            return -1;
        } else if (ret == 0) {
            return 0;
        } else {
            curr += ret;
        }
    }

    return 1;
}

int socketRecv(int sockfd, tsize_t *buf, tsize_t len) {
    int ret;
    tsize_t curr = 0;

    while (curr < len) {
        ret = recv(sockfd, &(buf[curr]), len, 0);
        if (ret == -1 || ret > len) {
            return -1;
        } else if (ret == 0) {
            return 0;
        } else {
            curr += ret;
        }
    }

    return 1;
}

int MSGReceive(int sockfd, tsize_t *tag, tsize_t *len, tsize_t *msg, tsize_t *ID_len, tsize_t *ID) {
    int ret;

    // recv MSG tag
    ret = socketRecv(sockfd, tag, 1);
    if (ret == 0 || *tag == QUIT) {
        return 0;
    } else if (ret == -1 || *tag != MSG_DATA) {
        return -1;
    }

    // recv length of ID
    ret = socketRecv(sockfd, ID_len, 1);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    // recv ID
    ret = socketRecv(sockfd, ID, *ID_len);
    if (ret == -1) return -1;

    // recv length of msg
    ret = socketRecv(sockfd, len, 1);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    // recv msg
    ret = socketRecv(sockfd, msg, *len);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    // truncate msg
    msg[*len] = '\0';

    return 1;
}

int MSGSend(int sockfd, tsize_t *tag, tsize_t *len, tsize_t *msg, tsize_t *ID_len, tsize_t *ID) {
    int ret;

    // send MSG tag
    ret = socketSend(sockfd, tag, 1);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    // send length of ID
    ret = socketSend(sockfd, ID_len, 1);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    // send ID
    ret = socketSend(sockfd, ID, *ID_len);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    // send length of msg
    ret = socketSend(sockfd, len, 1);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    // send msg
    ret = socketSend(sockfd, msg, *len);
    if (ret == -1) return -1;
    if (ret == 0) return 0;

    return 1;
}

/**************************************************************/
/********************* Helper Functions ***********************/
/**************************************************************/

void tCast(char *msg, tsize_t *t_msg, size_t max) {
    size_t i;
    for (i = 0; i < max && msg[i] != '\0'; i++) {
        t_msg[i] = (tsize_t) msg[i];
    }

    if (i >= max) {
        t_msg[max - 1] = '\0';
    } else {
        t_msg[i] = '\0';
    }

//    printf("i is %ld, tCast: %s\n", i, t_msg);
}

void socketErrorCheck(int ret, int sockfd, char *msg) {
    if (ret == -1) {
        perror(msg);
        close(sockfd);
        exit(1);
    }
}

void socketExitCheck(int ret, int sockfd, char *msg) {
    if (ret == 0) {
        printf("%s", msg);
        close(sockfd);
        exit(0);
    }
}

void t_socketExitCheck(int ret, int sockfd, char *msg) {
    if (ret == 0) {
        printf("sockfd %d closed.\n", sockfd);
        close(sockfd);
//        printf("SOCK: %d, TID: %lu\n", sockfd, (unsigned long) pthread_self());
        pthread_exit(NULL);
    }
}

void t_socketErrorCheck(int ret, int sockfd, char *msg) {
    if (ret == -1) {
        printf("Failed to receive from socket %d.\n", sockfd);
        close(sockfd);
//        printf("SOCK: %d, TID: %lu\n", sockfd, (unsigned long) pthread_self());
        pthread_exit(NULL);
    }
}

char *getIP() {
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;

    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if (hostname == -1) {
        perror("gethostname error");
        exit(1);
    }

    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
    if (host_entry == NULL) {
        perror("gethostbyname error");
        exit(1);
    }

    // To convert an Internet network
    // address into ASCII string
    IPbuffer = inet_ntoa(*((struct in_addr *)
            host_entry->h_addr_list[0]));
    if (NULL == IPbuffer) {
        perror("inet_ntoa error");
        exit(1);
    }
    return IPbuffer;
}

int _atoi(tsize_t *str)
{
 int res = 0; // Initialize result

 // Iterate through all characters of input string and
 // update result
 for (int i = 0; str[i] != '\0'; ++i) {
     if (str[i]> '9' || str[i]<'0')
         return -1;
     res = res*10 + str[i] - '0';
 }

 // return result.
 return res;
}
