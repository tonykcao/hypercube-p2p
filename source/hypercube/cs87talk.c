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

int MSGReceive(int sockfd, tsize_t *tag, tsize_t *len, tsize_t *msg, tsize_t *ID_len, tsize_t *ID, tsize_t* hop_count) {
    int ret;

    // recv MSG tag
    ret = socketRecv(sockfd, tag, 1);
    if (ret == 0 || *tag == QUIT) {
        return 0;
    } else if (ret == -1) {
        printflush("While receiving tag\n");
        perror("While receiving tag\n");
        fflush(stderr);
        return -1;
    } else if (*tag != MSG_DATA) {
        printflush("When tag is not MSG_DATA, Received tag %d\n", *tag);
        return -2;
    }
//    printflush("Received tag %d\n", *tag);

    // recv length of ID
    ret = socketRecv(sockfd, ID_len, 1);
    if (ret == -1) {
        printflush("While receiving ID length\n");
        perror("While receiving ID length\n");
        fflush(stderr);
        return -1;
    }
    if (ret == 0) return 0;
//    printflush("Received ID length %d\n", *ID_len);

    // recv ID
    ret = socketRecv(sockfd, ID, *ID_len);
    if (ret == -1) {
        printflush("While receiving ID\n");
        perror("While receiving ID\n");
        fflush(stderr);
        return -1;
    }
    if (ret == 0) return 0;
//    printflush("Received ID %s\n", ID);

    // truncate ID
    ID[*ID_len] = '\0';

    // recv length of msg
    ret = socketRecv(sockfd, len, 1);
    if (ret == -1) {
        printflush("While receiving msg length\n");
        perror("While receiving msg length\n");
        fflush(stderr);
        return -1;
    }
    if (ret == 0) return 0;
//    printflush("Received msg length %d\n", *len);

    // recv msg
    ret = socketRecv(sockfd, msg, *len);
    if (ret == -1) {
        printflush("While receiving msg\n");
        perror("While receiving msg\n");
        fflush(stderr);
        return -1;
    }
    if (ret == 0) return 0;
//    printflush("Received msg %s\n", msg);

    // truncate msg
    msg[*len] = '\0';

    // recv hop count
    ret = socketRecv(sockfd, hop_count, 1);
    if (ret == -1) {
        printflush("While receiving hop count\n");
        perror("While receiving hop count\n");
        fflush(stderr);
        return -1;
    }
    if (ret == 0) return 0;
//    printflush("Received hop count %d\n", *hop_count);

    return 1;
}

int MSGSend(int sockfd, tsize_t *tag, tsize_t *len, tsize_t *msg, tsize_t *ID_len, tsize_t *ID, tsize_t *hop_count) {
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

    // send hop count
    ret = socketSend(sockfd, hop_count, 1);
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

int _atoi(tsize_t *str) {
    int res = 0; // Initialize result

    // Iterate through all characters of input string and
    // update result
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] > '9' || str[i] < '0')
            return -1;
        res = res * 10 + str[i] - '0';
    }

    // return result.
    return res;
}

void idToBin(tsize_t val, tsize_t *dest) {
    for (int n = 0; n < 8; n++) {
        dest[7 - n] = (val >> n) & 1;
    }
}

int power(int base, unsigned int exp) {
    int i, result = 1;
    for (i = 0; i < exp; i++)
        result *= base;
    return result;
}

tsize_t binToID(tsize_t *binary) {
    int id = 0;
    for (int i = 0; i < 8; i++) {
        if (binary[7 - i] == 1) {
            id += power(2, i);
        }
    }
    return (tsize_t) id;
}

void getNeighbors(tsize_t id, tsize_t *neighbors) {
    for (int i = 0; i < MAX_NEIGHBORS; ++i) {
        // Flip the ith bit using XOR operation
        neighbors[i] = id ^ (1 << i);
    }
}
