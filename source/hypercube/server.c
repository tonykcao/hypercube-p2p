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

/*********************************************/
int main(int argc, char **argv) {
    printflush("Server IP: %s \n\n", getIP());
    fflush(stdout);
    int listenfd, confd, ret;
    struct sockaddr_in saddr, caddr;
    struct linger linger_val;
    int optval;
    unsigned int socklen;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("Create Socket Failed\n");
        exit(1);
    }

    linger_val.l_onoff = 0;
    linger_val.l_linger = 0;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_LINGER, (void *) &linger_val,
                     (socklen_t)(sizeof(struct linger)));
    if (ret < 0) {
        perror("Setting socket option failed");
        close(listenfd);
        exit(1);
    }
    optval = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (ret < 0) {
        perror("Setting socket option failed");
        close(listenfd);
        exit(1);
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(CS87_TALKSERVER_PORTNUM);
    saddr.sin_addr.s_addr = INADDR_ANY;
//    printflush("listen socket %d\n", listenfd);
    ret = bind(listenfd, (struct sockaddr *) &saddr, sizeof(saddr));
    if (ret == -1) {
        perror("Binding failed");
        close(listenfd);
        exit(1);
    }

    ret = listen(listenfd, BACKLOG);
    if (ret == -1) {
        perror("listen failed\n");
        close(listenfd);
        exit(1);
    }

    socklen = sizeof(caddr);
    pthread_mutex_init(&mutex, NULL);

    // initialize client map
    initializeMap();

    while (1) {

        confd = accept(listenfd, (struct sockaddr *) &caddr, &socklen);
        printflush("Accepted new client socket %d.\n", confd);
        if (confd == -1) {
            close(confd);
            exit(0);
        }

        pthread_t new_client;
        pthread_create(&new_client, NULL, clientWrapper, &confd);
    }

    close(listenfd);
}

/*********************************************/

int helloCheck(struct Client *client) {
    int ret;
    tsize_t tag;

    pthread_mutex_lock(&mutex);
    if (num_clients >= MAX_CLIENTS) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    pthread_mutex_unlock(&mutex);

    // recv HELLO_MSG tag
    ret = socketRecv(client->socket_fd, &tag, 1);
    if (ret == -1 || tag != HELLO_MSG) {
        perror("recv tag failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // recv length of name
    ret = socketRecv(client->socket_fd, &client->name_len, 1);
    if (ret == -1) {
        perror("recv len failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // recv name
    ret = socketRecv(client->socket_fd, client->name, client->name_len);
    if (ret == -1) {
        perror("recv name failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    client->name[client->name_len] = '\0';

    // recv ip len
    ret = socketRecv(client->socket_fd, &client->ip_len, 1);
    if (ret == -1) {
        perror("recv ip len failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // receive ip
    ret = socketRecv(client->socket_fd, client->ip, client->ip_len);
    if (ret == -1) {
        perror("recv ip failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    client->ip[client->ip_len] = '\0';

    // receive port len
    ret = socketRecv(client->socket_fd, &client->port_len, 1);
    if (ret == -1) {
        perror("recv port failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    // receive port
    ret = socketRecv(client->socket_fd, client->port, client->port_len);
    if (ret == -1) {
        perror("recv port failed\n");
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    client->port[client->port_len] = '\0';

    return 1;
}

void addClient(struct Client *client) {
    client->id = minID;
    clientMap[minID] = *client;
    clientMap[minID].occupied = 1;

    // update maxID
    if (minID > maxID) {
        maxID = minID;
    }

    // update minID
    updateMINID();
}

void removeClient(tsize_t id) {
    clientMap[id].occupied = -1;
    num_clients--;

    // update minID
    if (id < minID) {
        minID = id;
    }

    // update maxID by find the last occupied spot from the input id
    if (id == maxID) {
        updateMAXID();
    }
}

void *clientWrapper(void *arg) {
    // setup communication session
    int confd = *((int *) arg);
    int ret;

    // create a new client
    struct Client current_client;
    current_client.socket_fd = confd;

    // verify HELLO_MSG
    tsize_t reply_tag;
    ret = helloCheck(&current_client);

    if (ret == -1) {
        reply_tag = HELLO_ERROR;
        ret = socketSend(current_client.socket_fd, &reply_tag, 1);
        if (ret == -1) {
            printflush("Client (%s): send HELLO_ERROR tag failed\n", current_client.name);
        } else if (ret == 0) {
            printflush("Client (%d: %s) quit.\n", current_client.socket_fd, current_client.name);
        }
        perror("New client didn't passed HELLO check.\n");
        close(current_client.socket_fd);
        pthread_exit(NULL);
    } else if (ret == 0) {
        printflush("Client (%d: %s) quit.\n", current_client.socket_fd, current_client.name);
        close(current_client.socket_fd);
        pthread_exit(NULL);
    } else {
        reply_tag = HELLO_OK;
        ret = socketSend(current_client.socket_fd, &reply_tag, 1);
        if (ret == -1) {
            printflush("Client (%s): send HELLO_OK tag failed\n", current_client.name);
        } else if (ret == 0) {
            printflush("Client (%d: %s) quit.\n", current_client.socket_fd, current_client.name);
            close(current_client.socket_fd);
            pthread_exit(NULL);
        }
    }

    // print current client info
    printflush("New client passed HELLO check. \n"
               "Detailed Info: (%d) %s | (%d) %s | port: %s \n",
               current_client.name_len,
               current_client.name,
               current_client.ip_len,
               current_client.ip,
               current_client.port);

    /*********************************************/

    // add the client to the array
    pthread_mutex_lock(&mutex);
    addClient(&current_client);
    pthread_mutex_unlock(&mutex);

    tsize_t neighbors[MAX_NEIGHBORS];
    getNeighbors(current_client.id, neighbors);

    // send ID
    ret = socketSend(current_client.socket_fd, &current_client.id, 1);
    socketCheck(ret, current_client.socket_fd, "Send ID to a client\n", 2, current_client.id);
    printflush("Assigning ID [%d] to %s\n", current_client.id, current_client.name);

    // send other clients info
    sendClients(&current_client, neighbors);

    tsize_t check_tag;

    while (1) {
        // Check if the client is still alive
        int ret = socketRecv(current_client.socket_fd, &check_tag, 1);
        if (check_tag == RECONNECT) {
            printflush("Client id: %d | sockfd: %d | name: %s starts reconnection.\n", current_client.id,
                       current_client.socket_fd, current_client.name);
            close(current_client.socket_fd);
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(current_client.id);
            pthread_mutex_unlock(&mutex);
            break;
        } else if (ret == 0 || check_tag == QUIT) {
            printflush("Client id: %d | sockfd: %d | name: %s quit.\n", current_client.id,
                       current_client.socket_fd, current_client.name);
            close(current_client.socket_fd);
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(current_client.id);
            repairTopology(current_client.id);
            pthread_mutex_unlock(&mutex);
            break;
        } else if (ret == -1) {
            printflush("Client id: %d | sockfd: %d | name: %s receive message fails.\n", current_client.id,
                       current_client.socket_fd, current_client.name);
            perror("recv msg failed\n");
            close(current_client.socket_fd);
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(current_client.id);
            repairTopology(current_client.id);
            pthread_mutex_unlock(&mutex);
            break;
        }
    }

    pthread_exit(NULL);
}

void sendClients(struct Client *current, tsize_t *neighbors) {
    int ret;
    tsize_t num_neighbors = 0;

    // find the number of neighbors
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (clientMap[neighbors[i]].occupied != -1) {
            num_neighbors++;
        }
    }

    ret = socketSend(current->socket_fd, &num_neighbors, 1);
    socketCheck(ret, current->socket_fd, "Send num of neighbors to a client\n", 0, current->id);
    printflush("Send %d clients to %s\n", num_neighbors, current->name);

    // for each client, send client info
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        // pass empty spots
        if (clientMap[neighbors[i]].occupied == -1) {
            continue;
        }

        printflush("Client [%d] info: id: %d | (%d) %s | (%d) %s | (%d) port: %s\n",
                   i,
                   clientMap[neighbors[i]].id,
                   clientMap[neighbors[i]].name_len,
                   clientMap[neighbors[i]].name,
                   clientMap[neighbors[i]].ip_len,
                   clientMap[neighbors[i]].ip,
                   clientMap[neighbors[i]].port_len,
                   clientMap[neighbors[i]].port);

        // send ID
        ret = socketSend(current->socket_fd, &clientMap[neighbors[i]].id, 1);
        socketCheck(ret, current->socket_fd, "Send ID to a client\n", 0, current->id);

        // send name len
        ret = socketSend(current->socket_fd, &clientMap[neighbors[i]].name_len, 1);
        socketCheck(ret, current->socket_fd, "Send name len to a client\n", 0, current->id);

        // send name
        ret = socketSend(current->socket_fd, clientMap[neighbors[i]].name, clientMap[neighbors[i]].name_len);
        socketCheck(ret, current->socket_fd, "send name to a client\n", 0, current->id);

        // send ip len
        ret = socketSend(current->socket_fd, &clientMap[neighbors[i]].ip_len, 1);
        socketCheck(ret, current->socket_fd, "send ip len to a client\n", 0, current->id);

        // send ip
        ret = socketSend(current->socket_fd, clientMap[neighbors[i]].ip, clientMap[neighbors[i]].ip_len);
        socketCheck(ret, current->socket_fd, "send ip to a client\n", 0, current->id);

        // send port len
        ret = socketSend(current->socket_fd, &clientMap[neighbors[i]].port_len, 1);
        socketCheck(ret, current->socket_fd, "send port len to a client\n", 0, current->id);

        // send port
        ret = socketSend(current->socket_fd, clientMap[neighbors[i]].port, clientMap[neighbors[i]].port_len);
        socketCheck(ret, current->socket_fd, "send port to a client\n", 0, current->id);
    }
    printflush("Sent all existing neighbors to %s\n\n", current->name);
}

void socketCheck(int ret, int sockfd, char *msg, int mode, int id) {
    if (ret == 0) {
        printflush("\n\033[A\rThe target socket %d has closed.", sockfd);
        printflush("%s\n$: ", msg);

        if (mode == 1) {
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(id);
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            exit(0);
        } else if (mode == 2) {
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(id);
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            pthread_exit(NULL);
        }
    } else if (ret == -1) {
        printflush("The target socket %d has failed. The specific check message: ", sockfd);
        perror(msg);

        if (mode == 1) {
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(id);
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            exit(1);
        } else if (mode == 2) {
            // remove client from array
            pthread_mutex_lock(&mutex);
            removeClient(id);
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            pthread_exit(NULL);
        }
    }
}

void initializeMap() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientMap[i].occupied = -1;
    }
}

void printNeighbors(tsize_t *neighbors) {
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        printflush("Neighbor %d: %d\n", i, neighbors[i]);
        printflush("Client [%d] info: (%d) %s | (%d) %s | (%d) port: %s\n",
                   i,
                   clientMap[neighbors[i]].name_len,
                   clientMap[neighbors[i]].name,
                   clientMap[neighbors[i]].ip_len,
                   clientMap[neighbors[i]].ip,
                   clientMap[neighbors[i]].port_len,
                   clientMap[neighbors[i]].port);
    }
}

void updateMINID() {
    // check the first empty spot
    for (int i = 0; i < num_clients; i++) {
        if (clientMap[i].occupied == -1) {
            minID = i;
            break;
        }
    }
}

void updateMAXID() {
    // check the last occupied spot
    for (int i = maxID - 1; i >= 0; i--) {
        if (clientMap[i].occupied != -1) {
            maxID = i;
            break;
        }
    }
}

void repairTopology(tsize_t id) {
    if (id > maxID || id == 0) return;
    int ret;
    tsize_t tag = RECONNECT;
    ret = socketSend(clientMap[maxID].socket_fd, &tag, 1);
    socketCheck(ret, clientMap[maxID].socket_fd, "Send RECONNECT tag to a client\n", 0, clientMap[maxID].id);
    printflush("Sent RECONNECT tag to %s with ID = [%d]\n", clientMap[maxID].name, clientMap[maxID].id);
    updateMAXID();
}