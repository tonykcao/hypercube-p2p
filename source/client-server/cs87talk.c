#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "cs87talk.h"

/**************************************************************/

int send_msg(int sockfd, tsize_t *buf, tsize_t len) {
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

int recv_msg(int sockfd, tsize_t *buf, tsize_t len) {
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

void t_cast(char *msg, tsize_t *t_msg, int max) {
    for(int i = 0; i < max && msg[i] != '\0' && msg[i] != '\n'; i++) {
        t_msg[i] = (tsize_t) msg[i];
    }

    t_msg[strlen(msg)] = '\0';
}

int MSGReceive(int confd, tsize_t *tag, tsize_t *len, tsize_t *msg) {
    int ret;

    // recv MSG tag
    ret = recv_msg(confd, tag, 1);
    if (ret == -1) {
        perror("recv tag failed\n");
        return -1;
    } else if (ret == 0 || *tag == QUIT) {
        return 0;
    } else if (*tag != MSG_DATA) {
        perror("recv tag failed\n");
        return -1;
    }

    // recv length of msg
    ret = recv_msg(confd, len, 1);
    if (ret == -1) {
        perror("recv len failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // recv msg
    ret = recv_msg(confd, msg, *len);
    if (ret == -1) {
        perror("recv msg failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    msg[*len] = '\0';
    return 1;
}

int MSGSend(int confd, tsize_t *tag, tsize_t *len, tsize_t *msg) {
    int ret;

    // send MSG tag
    ret = send_msg(confd, tag, 1);
    if (ret == -1) {
        perror("send tag failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // send length of msg
    ret = send_msg(confd, len, 1);
    if (ret == -1) {
        perror("send len failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // send msg
    ret = send_msg(confd, msg, *len);
    if (ret == -1) {
        perror("send msg failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    return 1;
}