#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#include "cs87talk.h"
#include "server.h"

static struct Client clients[MAX_CLIENTS];
static int num_clients;
static pthread_mutex_t mutex;

/*********************************************/
int main(int argc, char **argv) {
    printflush("Server IP: %s \n", getIP());
    fflush(stdout);
    int listenfd, confd, ret;
    struct sockaddr_in saddr, caddr;
    struct linger linger_val;
    int optval;
    unsigned int socklen;

    // step (0) init any server-side data structs

    // step (1) create a TCP socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("Create Socket Failed\n");
        exit(1);
    }

    // set socket opts on the listen socket SO_LINGER off and SO_REUSEADDR
    // here is the code to do it (it is complete)
    linger_val.l_onoff = 0;
    linger_val.l_linger = 0;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_LINGER, (void *) &linger_val,
                     (socklen_t)(sizeof(struct linger)));
    if (ret < 0) {
        perror("Setting socket option failed");
        close(listenfd);
        exit(1);
    }
    // set SO_REUSEADDR on a socket to true (1):
    optval = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (ret < 0) {
        perror("Setting socket option failed");
        close(listenfd);
        exit(1);
    }

    // setp (2) bind to port CS87_TALKSERVER_PORTNUM
    // (a) create and init an address struct (this is complete)
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(CS87_TALKSERVER_PORTNUM);
    saddr.sin_addr.s_addr = INADDR_ANY;
    // TODO: uncomment bind call
    printflush("listen socket %d\n", listenfd);
    ret = bind(listenfd, (struct sockaddr *) &saddr, sizeof(saddr));
    if (ret == -1) {
        perror("Binding failed");
        close(listenfd);
        exit(1);
    }

    // step (3) tell OS we are going to listen on this socket
    // BACKLOG specifies how much space the OS should reserve for incoming
    // connections that have not yet been accepted by this server.
    ret = listen(listenfd, BACKLOG);
    if (ret == -1) {
        perror("listen failed\n");
        close(listenfd);
        exit(1);
    }

    socklen = sizeof(caddr);

    // now the server is ready to accept connections from clients
    // printflush("socklen = %u sockaddr addr %u\n", socklen, saddr.sin_addr.s_addr);

    pthread_mutex_init(&mutex, NULL);
    while (1) {

        // step (4) accept the next connection from a client
        confd = accept(listenfd, (struct sockaddr *) &caddr, &socklen);
        // printflush("Accepted new client socket %d.\n", confd);
        // fflush(stdout);
        printflush("Accepted new client socket %d.\n", confd);
        if (confd == -1) {
            close(confd);
            exit(0);
        }

        pthread_t new_client;
        pthread_create(&new_client, NULL, CommunicationWrapper, &confd);
    }

    close(listenfd);
}

/*********************************************/
int helloCheck(int confd, tsize_t *tag, tsize_t *len, tsize_t *name, tsize_t *ip_len, tsize_t *ip, tsize_t *port_len,tsize_t *port) {
    int ret;

    pthread_mutex_lock(&mutex);
    if (num_clients >= MAX_CLIENTS) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    pthread_mutex_unlock(&mutex);

    // recv HELLO_MSG tag
    ret = socketRecv(confd, tag, 1);
    if (ret == -1 || *tag != HELLO_MSG) {
        perror("recv tag failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // recv length of name
    ret = socketRecv(confd, len, 1);

    if (ret == -1) {
        perror("recv len failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // recv name
    ret = socketRecv(confd, name, *len);
    if (ret == -1) {
        perror("recv name failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    name[*len] = '\0';

    // recv ip len
    ret = socketRecv(confd, ip_len, 1);
    if (ret == -1) {
        perror("recv ip len failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // receive ip
    ret = socketRecv(confd, ip, *ip_len);
    printflush("received ip: %s\n", ip);
    if (ret == -1) {
        perror("recv ip failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // receive port
    ret = socketRecv(confd, port_len, 1);
    // printflush("received port len: %d\n", (int)port_len);
    if (ret == -1) {
        perror("recv port failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // receive port
    ret = socketRecv(confd, port, *port_len);
    printflush("received port: %s\n", port);
    if (ret == -1) {
        perror("recv port failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    return 1;
}

// Function to add a new client to the array
void addClient(struct Client *newClient) {
    clients[num_clients] = *newClient;
    num_clients++;
}

// Function to remove a client by index
void removeClient(int socket_fd) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd == socket_fd) {
            // Shift elements after the removed index
            for (int j = i; j < num_clients - 1; j++) {
                clients[j] = clients[j + 1];
            }
            num_clients--;
            break;
        }
    }
}

// Function to broadcast a message to all clients
//void broadcast(tsize_t *msg, tsize_t len) {
//    tsize_t msg_tag = MSG_DATA;
//    tsize_t msg_len = len;
//
//    // printflush("Broadcasting: %s\n", msg);
//    for (int i = 0; i < num_clients; i++) {
//        int ret = MSGSend(clients[i].socket_fd, &msg_tag, &msg_len, msg);
//        if (ret == 0) {
//            printflush("Client (%d: %s) quit.\n", clients[i].socket_fd, clients[i].name);
//        } else if (ret == -1) {
//            printflush("broadcast msg failed at client %s\n", clients[i].name);
//        }
//    }
//}

void *CommunicationWrapper(void *arg) {
    // Setup communication session
    int confd = *((int *) arg);
    printflush("confd %d \n", confd);
    int ret;

    // create a new client
    struct Client current_client;
    current_client.socket_fd = confd;

    // Verify HELLO_MSG
    tsize_t tag, reply_tag;
    ret = helloCheck(confd, &tag, &current_client.name_len, current_client.name, &current_client.ip_len,
                        current_client.ip, &current_client.port_len ,current_client.port);

    if (ret == -1) {
        reply_tag = HELLO_ERROR;
        ret = socketSend(confd, &reply_tag, 1);
        if (ret == -1) {
            printflush("Client (%s): send HELLO_ERROR tag failed\n", current_client.name);
        } else if (ret == 0) {
            printflush("Client (%d: %s) quit.\n", current_client.socket_fd, current_client.name);
        }
        perror("New client didn't passed HELLO check.\n");
        close(confd);
        pthread_exit(NULL);
    } else if (ret == 0) {
        printflush("Client (%d: %s) quit.\n", current_client.socket_fd, current_client.name);
        close(confd);
        pthread_exit(NULL);
    } else {
        reply_tag = HELLO_OK;
        ret = socketSend(confd, &reply_tag, 1);
        if (ret == -1) {
            printflush("Client (%s): send HELLO_OK tag failed\n", current_client.name);
        } else if (ret == 0) {
            printflush("Client (%d: %s) quit.\n", current_client.socket_fd, current_client.name);
            close(confd);
            pthread_exit(NULL);
        }
        printflush("\nNew client (%s) passed HELLO check.\n", current_client.name);

        tsize_t buf[MSGMAX];
        buf[0] = num_clients;
        // send num_clients
        ret = socketSend(confd, buf, 1);
        printflush("about to send over %d clients, ret: %d\n", buf[0], ret);

        // for each client, send client info
        for (int i = 0; i < num_clients; i++)
        {
            printflush("Sending client [%d] info: (%d) %s  | (%d) %s | port: %s\n",
                i,
                clients[i].name_len,
                clients[i].name,
                clients[i].ip_len,
                clients[i].ip,
                clients[i].port
            );
            // send name len
            buf[0] = clients[i].name_len;
            // printflush("about to send over name len: %d", buf[0]);
            socketSend(confd, buf, 1);
            // send name
            // printflush("about to send over name : %s", clients[i].name);
            socketSend(confd, clients[i].name, clients[i].name_len);

            // send ip len
            buf[0] = clients[i].ip_len;
            // printflush("about to send over ip len : %d", buf[0]);
            socketSend(confd, buf, 1);
            // send ip
            // printflush("about to send over ip len : %s", clients[i].ip);
            socketSend(confd, clients[i].ip, clients[i].ip_len);


            buf[0] = clients[i].port_len;
            // send port len
            socketSend(confd, buf, 1);
            // send port 
            socketSend(confd, clients[i].port, clients[i].port_len);
        }

        // add the client to the array
        pthread_mutex_lock(&mutex);
        addClient(&current_client);
        pthread_mutex_unlock(&mutex);
    }

    tsize_t msg_tag, msg_len, ID_len;
    tsize_t msg[MSGMAX];
    tsize_t ID[MSGMAX];
    tsize_t combined_msg[MSGMAX];

    while (1) {
        // Receive MSG
        int ret = MSGReceive(confd, &msg_tag, &msg_len, msg, &ID_len, ID);
        if (ret == 0) {
            printflush("Client (%d: %s) quit.\n", current_client.socket_fd, current_client.name);
            close(confd);
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(current_client.socket_fd);
            pthread_mutex_unlock(&mutex);
            break;
        } else if (ret == -1) {
            perror("recv msg failed\n");
            close(confd);
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(current_client.socket_fd);
            pthread_mutex_unlock(&mutex);
            break;
        } else {
            // // cut off msg if too long
            // if (msg_len > MSGMAX - current_client.name_len - 2) {
            //     msg[MSGMAX - current_client.name_len - 2] = '\0';
            //     msg_len = MSGMAX - current_client.name_len - 2;
            // }

            // // combine name and msg
            // for (int i = 0; i < current_client.name_len; i++) {
            //     combined_msg[i] = current_client.name[i];
            // }
            // combined_msg[current_client.name_len] = ':';
            // combined_msg[current_client.name_len + 1] = ' ';
            // for (int i = 0; i < msg_len; i++) {
            //     combined_msg[i + current_client.name_len + 2] = msg[i];
            // }

            // // broadcast MSG
            // pthread_mutex_lock(&mutex);
            // broadcast(combined_msg, current_client.name_len + msg_len + 2);
            // pthread_mutex_unlock(&mutex);
        }

        // Clear MSG
        memset(msg, 0, sizeof(msg));
        memset(combined_msg, 0, sizeof(combined_msg));
    }

    pthread_exit(NULL);
}
