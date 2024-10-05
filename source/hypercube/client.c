#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#include "cs87talk.h"
#include "client.h"
#include <sys/time.h>
#include <signal.h>

int main(int argc, char **argv) {
    time_t t;
    srand((unsigned) time(&t));
    int iters = -1; // not in testing mode
    if (argc == 4) {
        iters = atoi(argv[3]);
        printflush("Testing mode, sending %d messages\n", iters);
    } else if (argc != 3) {
        printflush("Usage: ./cs87_client server_IP your_screen_name\n");
        exit(1);
    }

    // Initialize values
    int sockfd, ret;
    unsigned int listenfd;
    struct sockaddr_in saddr, caddr;
    struct linger linger_val;

    // Initialize ip and port
    char port_self[MSGMAX];
    char *server_ip = argv[1];
    char *name = argv[2];
    char *ip_self = getIP();
    if (strcmp(server_ip, "0") == 0) server_ip = getIP();

    // Print initial message
    initialPrint(argc, argv, ip_self, server_ip);

    // create TCP sockets
    sockfd = socket(PF_INET, SOCK_STREAM, 0); // for establishing outbound connection to SP
    socketErrorCheck(sockfd, sockfd, "Create outbound socket failed");
    listenfd = socket(PF_INET, SOCK_STREAM, 0); // for accepting inbound connections
    socketErrorCheck(listenfd, listenfd, "Create inbound socket failed");

    // set up listenfd socket
    linger_val.l_onoff = 0;
    linger_val.l_linger = 0;
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, (void *) &linger_val,
               (socklen_t)(sizeof(struct linger))); // inbound settings

    saddr.sin_port = htons(CS87_TALKSERVER_PORTNUM); // SP address setting
    saddr.sin_family = AF_INET;

    // client should pick an available port to listen on
    caddr.sin_family = AF_INET;
    caddr.sin_port = 0;
    caddr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(listenfd, (struct sockaddr *) &caddr, sizeof(caddr));
    socketErrorCheck(ret, listenfd, "Binding to port failed\n");
    socklen_t _len = sizeof(caddr);
    if (getsockname(listenfd, (struct sockaddr *) &caddr, &_len) == -1) {
        perror("error when getting the sock name");
        exit(1);
    }
    sprintf(port_self, "%d", ntohs(caddr.sin_port)); // cast port number to string

    // Connect to the SP
    if (!inet_aton(server_ip, &(saddr.sin_addr))) {
        perror("inet_aton");
        exit(1);
    }
    ret = connect(sockfd, (struct sockaddr *) &saddr, sizeof(saddr));
    socketErrorCheck(ret, sockfd, "Connect Failed\n");

    /******************************************************************/

    // Create struct for the current client
    struct Client current;
    tCast(name, current.name, NAMEMAX);
    tCast(ip_self, current.ip, MSGMAX);
    tCast(port_self, current.port, MSGMAX);
    current.socket_fd = sockfd;
    current.name_len = strlen(name) >= NAMEMAX ? NAMEMAX - 1 : strlen(name);
    current.ip_len = strlen(ip_self);
    current.port_len = strlen(port_self);

    // HELLO protocol
    helloCheck(&current);
    printflush("Client Connected to the SP. ");

    // Receive ID
    ret = socketRecv(sockfd, &current.id, 1);
    socketCheck(ret, sockfd, "Recv ID from the SP\n", 1);
    printflush("Your Client ID: %d\n", current.id);

    // Receive list of other clients
    receiveClients(sockfd, &current);

//    // Create SPReceiveWrapper thread
    struct SPPackage spPackage;
    spPackage.server_ip = server_ip;
    spPackage.iters = iters;
    spPackage.client = &current;

    pthread_create(&sp_pid, NULL, SPReceiveWrapper, &spPackage);

    // Create send thread
    struct sendPackage sendPackage;
    sendPackage.client = &current;
    sendPackage.iters = iters;

    pthread_create(&send_pid, NULL, MSGSendWrapper, &sendPackage);

    // Create thread for listening to new clients
    struct listenPackage listenPackage;
    listenPackage.listenfd = listenfd;

    pthread_create(&listen_pid, NULL, addNewClientWrapper, &listenPackage);

    // Join send thread
    pthread_join(send_pid, NULL);

//    printflush("After quitting send thread\n");

    // Cancel addNewClientWrapper thread
    pthread_cancel(listen_pid);
    pthread_join(listen_pid, NULL);

//    printflush("After quitting addNewClientWrapper thread\n");

    // Cancel SP thread
    pthread_join(sp_pid, NULL);

//    printflush("After quitting SP thread\n");

//    // Join all listen threads
//    while (listen_count > 0) {
//        pthread_join(listen_threads[0], NULL);
//    }

    // Clean variables
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_var);
    pthread_cond_destroy(&cond_var_main);

    printflush("\r\033[KQuit current client. Bye bye.\n");
}

void helloCheck(struct Client *client) {
    int ret;
    tsize_t tag = HELLO_MSG;
    tsize_t reply_tag = 0;

    // send Hello tag
    ret = socketSend(client->socket_fd, &tag, 1);
    socketCheck(ret, client->socket_fd, "Send hello tag to the SP\n", 1);

    // send length of msg
    ret = socketSend(client->socket_fd, &client->name_len, 1);
    socketCheck(ret, client->socket_fd, "Send name_len to the SP\n", 1);

    // send name
    ret = socketSend(client->socket_fd, client->name, client->name_len);
    socketCheck(ret, client->socket_fd, "Send name to the SP\n", 1);

    // send length of ip
    ret = socketSend(client->socket_fd, &client->ip_len, 1);
    socketCheck(ret, client->socket_fd, "Send ip_len to the SP\n", 1);

    // send ip
    ret = socketSend(client->socket_fd, client->ip, client->ip_len);
    socketCheck(ret, client->socket_fd, "Send ip to the SP\n", 1);

    // send port len
    ret = socketSend(client->socket_fd, &client->port_len, 1);
    socketCheck(ret, client->socket_fd, "Send port_len to the SP\n", 1);

    // send port number
    ret = socketSend(client->socket_fd, client->port, client->port_len);
    socketCheck(ret, client->socket_fd, "Send port to the SP\n", 1);

    // recv reply tag
    ret = socketRecv(client->socket_fd, &reply_tag, 1);
    socketCheck(ret, client->socket_fd, "Recv reply tag from the SP\n", 1);

    // check reply tag
    if (reply_tag != HELLO_OK) {
        printflush("SP rejects the connection.\n bye, bye\n");
        close(client->socket_fd);
        exit(0);
    }
}

void *MSGReceiveWrapper(void *arg) {
    pthread_mutex_lock(&mutex);
    waiting_threads++;
    pthread_cond_signal(&cond_var_main);  // Signal main thread
    while (ready_to_start == 0) {
        pthread_cond_wait(&cond_var, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    struct receiveThreadPackage *receiveThreadPackage;
    receiveThreadPackage = (struct receiveThreadPackage *) arg;
    pthread_t current_thread = pthread_self();

    int ret;
    int socket_fd = receiveThreadPackage->socket_fd;

    struct receiveMSGPackage receiveMSGPackage;
    receiveMSGPackage.socket_fd = socket_fd;

    printflush("Listening to [socket %d] %s\n", socket_fd, receiveThreadPackage->name);

    while (1) {
        pthread_mutex_lock(&mutex);
        if (reconnect == 1) {
            pthread_mutex_unlock(&mutex);
            free(receiveThreadPackage);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&mutex);

        // Receive MSG
        ret = MSGReceive(socket_fd, &receiveMSGPackage.msg_tag, &receiveMSGPackage.msg_len,
                         receiveMSGPackage.msg, &receiveMSGPackage.ID_len, receiveMSGPackage.ID, &receiveMSGPackage.hop_count);
        if (ret == -1) {
            printflush("The connection to target [socket %d] %s has failed.",
                       socket_fd, receiveThreadPackage->name);
            perror("Receive msg failed\n");
            fflush(stderr);

            // remove this client from the clients array and the listen_threads array
            pthread_mutex_lock(&mutex);
            updateClientList(socket_fd);
            updateListenThreadList(current_thread);
            close(socket_fd);
            pthread_mutex_unlock(&mutex);

            free(receiveThreadPackage);
            pthread_exit(NULL);
        } else if (ret == 0) {
            printflush("\n\033[A\rThe connection to [socket %d] %s has closed.\n$: ",
                       socket_fd, receiveThreadPackage->name);

            // remove this client from the clients array and the listen_threads array
            pthread_mutex_lock(&mutex);
            updateClientList(socket_fd);
            updateListenThreadList(current_thread);
            close(socket_fd);
            pthread_mutex_unlock(&mutex);

            free(receiveThreadPackage);
            pthread_exit(NULL);
        } else if (ret == -2) {
            printflush("The tag is not MSG_DATA. It is %d. It comes from client %s with socket [%d].\n",
                       receiveMSGPackage.msg_tag, receiveThreadPackage->name, socket_fd);
            // Clear MSG
            memset(&receiveMSGPackage, 0, sizeof(receiveMSGPackage));
            receiveMSGPackage.socket_fd = socket_fd;
            continue;
        }

        // special_cmd(socket_fd, (char *) receiveMSGPackage.msg);
        processReceivedMSG(receiveMSGPackage);

        // Clear MSG
        memset(&receiveMSGPackage, 0, sizeof(receiveMSGPackage));
        receiveMSGPackage.socket_fd = socket_fd;
        receiveMSGPackage.msg_len = 0;
        receiveMSGPackage.ID_len = 0;
        receiveMSGPackage.hop_count = 0;
        // reset msg
        memset(receiveMSGPackage.msg, 0, sizeof(receiveMSGPackage.msg));
        memset(receiveMSGPackage.ID, 0, sizeof(receiveMSGPackage.ID));
    }

    free(receiveThreadPackage);
    pthread_exit(NULL);
}

void *MSGSendWrapper(void *arg) {
    struct sendPackage *send_args;
    send_args = (struct sendPackage *) arg;

    int ret;
    size_t input_len;
    int sockfd = send_args->client->socket_fd;
    int iters = send_args->iters;

    int count = 0;
    int should_send = (iters != -1) ? (count < iters) : 1;

    while (should_send) {
        char *input = NULL;

        if (iters != -1) {
            input = malloc(sizeof(char) * MSGMAX);
            sleep(1);
            snprintf(input, MSGMAX, "msg %d", count);
            if (count == iters) {
                pthread_mutex_lock(&mutex);
                viewLog();
                pthread_mutex_unlock(&mutex);
                snprintf(input, MSGMAX, "\\goodbye");
            }
            count++;
            should_send = count <= iters;
        } else {
            input = readline("$: ");
            while (strlen(input) == 0) {
                free(input);
                input = readline(NULL);
            }
        }

        if (input == NULL) {
            close(sockfd);
            free(input);
            pthread_exit(NULL);
        }

        input_len = strlen(input);

        if (input_len > MSGMAX) {
            input[MSGMAX] = '\0';
            input_len = MSGMAX;
        }

        tsize_t msg_buffer[MSGMAX];
        tsize_t msg_len = input_len;
        tCast(input, msg_buffer, MSGMAX);

        // create MSG ID
        tsize_t ID_buffer[MSGMAX];
        createMSGID(ID_buffer, MSGMAX, send_args->client->name);
        tsize_t ID_len = sizeof(ID_buffer);

        if (strcmp(input, "\\goodbye") == 0 || strcmp(input, "\\quit") == 0 || strcmp(input, "\\exit") == 0) {
            printflush("quitting...\n");
            tsize_t quit_tag = QUIT;
            ret = socketSend(sockfd, &quit_tag, 1);
            if (ret == -1) perror("Send quit tag to SP failed\n");
            close(sockfd);

            pthread_mutex_lock(&mutex);
            ready_to_quit = 1;
            // Close with other clients
            for (int i = 0; i < peer_count; i++) {
                ret = socketSend(clients[i].socket_fd, &quit_tag, 1);
            }
            peer_count = 0;
            // Close all listen threads
            for (int i = 0; i < listen_count; i++) {
                pthread_cancel(listen_threads[i]);
                printf("After canceling thread %ld\n", listen_threads[i]);
//                pthread_join(listen_threads[i], NULL);
            }
            listen_count = 0;
            pthread_mutex_unlock(&mutex);
            free(input);
            printflush("Finished disconnections. Bye bye.\n");
            pthread_exit(NULL);
        }

        if (strcmp(input, "\\log") == 0) {
            pthread_mutex_lock(&mutex);
            viewLog();
            pthread_mutex_unlock(&mutex);
            free(input);
            continue;
        } else if (strcmp(input, "\\clear") == 0) {
            printf("\033[H\033[J");
            free(input);
            continue;
        } else if (strcmp(input, "\\help") == 0) {
            viewUserCommands();
            free(input);
            continue;
        }

        pthread_mutex_lock(&mutex);
        while (reconnect == 1) {
            pthread_cond_wait(&cond_var_main, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        // initialize MSG Protocol
        tsize_t msg_tag = MSG_DATA;
        // initialize hop count
        tsize_t hop_count = 0;

        // print everything needed to send below in one print statement
        printflush("About to send message: tag=%d len=%d msg=%s ID=%s hop=%d\n",
                   msg_tag,
                   msg_len,
                   input,
                   (char *) ID_buffer,
                   hop_count);

        // Send MSG
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < peer_count; i++) {
            ret = MSGSend(clients[i].socket_fd, &msg_tag, &msg_len, msg_buffer, &ID_len, ID_buffer, &hop_count);
        }
        pthread_mutex_unlock(&mutex);

        // log the message by calling processSendMSG
        processSendMSG(ID_buffer, msg_buffer);

        // Clear MSG
        memset(msg_buffer, 0, sizeof(msg_buffer));
        memset(ID_buffer, 0, sizeof(ID_buffer));
        free(input);
    }
    pthread_exit(NULL);
}

void receiveClients(int sockfd, struct Client *current) {
    int ret;
    struct sockaddr_in addr;

    // Receive number of clients
    ret = socketRecv(sockfd, &peer_count, 1);
    socketCheck(ret, sockfd, "Recv number of other clients from the SP\n", 1);
    printflush("Number of neighbors in the chat: %d. Begin to establish connections.\n", peer_count);

    // Receive clients from SP
    for (int i = 0; i < peer_count; i++) {
        // recv id
        ret = socketRecv(sockfd, &clients[i].id, 1);
        socketCheck(ret, sockfd, "Recv id from another client\n", 0);

        // recv name len
        ret = socketRecv(sockfd, &clients[i].name_len, 1);
        socketCheck(ret, sockfd, "Recv name_len from another client\n", 0);

        // recv name
        ret = socketRecv(sockfd, clients[i].name, clients[i].name_len);
        clients[i].name[clients[i].name_len] = '\0';
        socketCheck(ret, sockfd, "Recv name from another client\n", 0);

        // recv ip len
        ret = socketRecv(sockfd, &clients[i].ip_len, 1);
        socketCheck(ret, sockfd, "Recv ip_len from another client\n", 0);

        // recv ip
        ret = socketRecv(sockfd, clients[i].ip, clients[i].ip_len);
        clients[i].ip[clients[i].ip_len] = '\0';
        socketCheck(ret, sockfd, "Recv ip from another client\n", 0);

        // recv port len
        ret = socketRecv(sockfd, &clients[i].port_len, 1);
        socketCheck(ret, sockfd, "Recv port_len from another client\n", 0);

        // recv port number
        ret = socketRecv(sockfd, clients[i].port, clients[i].port_len);
        clients[i].port[clients[i].port_len] = '\0';
        socketCheck(ret, sockfd, "Recv port from another client\n", 0);
    }
    printflush("Received all existing neighbors. Begin to connect...\n");

    pthread_mutex_lock(&mutex);
    // Create thread for receiving new clients
    for (int i = 0; i < peer_count; i++) {
        /***** Connect to the current client *****/
        clients[i].socket_fd = socket(PF_INET, SOCK_STREAM, 0);
        addr.sin_port = htons(_atoi(clients[i].port));
        addr.sin_family = AF_INET;
        if (!inet_aton((char *) clients[i].ip, &(addr.sin_addr))) {
            perror("Connecting to one of other clients failed");
        }

        ret = connect(clients[i].socket_fd, (struct sockaddr *) &addr, sizeof(addr));
        socketErrorCheck(ret, clients[i].socket_fd, "Connect to a client failed\n");

        // send id to client
        ret = socketSend(clients[i].socket_fd, &current->id, 1);
        socketCheck(ret, clients[i].socket_fd, "Send id to another client\n", 0);

        // send name len to client
        ret = socketSend(clients[i].socket_fd, &current->name_len, 1);
        socketCheck(ret, clients[i].socket_fd, "Send name_len to another client\n", 0);

        // send name to client
        ret = socketSend(clients[i].socket_fd, current->name, current->name_len);
        socketCheck(ret, clients[i].socket_fd, "Send name to another client\n", 0);

        // send ip len to client
        ret = socketSend(clients[i].socket_fd, &current->ip_len, 1);
        socketCheck(ret, clients[i].socket_fd, "Send ip_len to another client\n", 0);

        // send ip to client
        ret = socketSend(clients[i].socket_fd, current->ip, current->ip_len);
        socketCheck(ret, clients[i].socket_fd, "Send ip to another client\n", 0);

        // send port len to client
        ret = socketSend(clients[i].socket_fd, &current->port_len, 1);
        socketCheck(ret, clients[i].socket_fd, "Send port_len to another client\n", 0);

        // send port to client
        ret = socketSend(clients[i].socket_fd, current->port, current->port_len);
        socketCheck(ret, clients[i].socket_fd, "Send port to another client\n", 0);

        printflush("Neighbor [%d] Info: id: %d | (%d) %s | (%d) %s | (%d) port: %s\n",
                   clients[i].socket_fd,
                   clients[i].id,
                   clients[i].name_len,
                   clients[i].name,
                   clients[i].ip_len,
                   clients[i].ip,
                   clients[i].port_len,
                   clients[i].port);

        struct receiveThreadPackage *receiveThreadPackage = malloc(sizeof(struct receiveThreadPackage));
        receiveThreadPackage->socket_fd = clients[i].socket_fd;
        memcpy(receiveThreadPackage->name, clients[i].name, NAMEMAX);
        printflush("Creating thread for [socket %d] %s\n", receiveThreadPackage->socket_fd, receiveThreadPackage->name);

        pthread_t recv_pid;
        pthread_create(&recv_pid, NULL, MSGReceiveWrapper, receiveThreadPackage);
        listen_threads[listen_count] = recv_pid;
        listen_count++;
    }

    // Wait until all worker threads are ready and waiting
    while (waiting_threads < peer_count) {
        pthread_cond_wait(&cond_var_main, &mutex);
    }
    ready_to_start = 1;
    pthread_cond_broadcast(&cond_var);
    pthread_mutex_unlock(&mutex);
    printflush("All neighbors are initialized.\n");
}

void *addNewClientWrapper(void *arg) {
    struct listenPackage *listen_args;
    listen_args = (struct listenPackage *) arg;

    int listenfd = listen_args->listenfd;
    struct sockaddr_in _addr;

    int ret, confd = 0;
    unsigned int socklen = sizeof(_addr);

    ret = listen(listenfd, BACKLOG);
    socketErrorCheck(ret, listenfd, "Listen failed\n");

    while (1) {
        confd = accept(listenfd, (struct sockaddr *) &_addr, &socklen);
        if (confd == -1) {
            perror("failed at client accept");
            continue;
        }

        struct Client current_client;
        current_client.socket_fd = confd;

        // recv id
        ret = socketRecv(current_client.socket_fd, &current_client.id, 1);
        socketCheck(ret, current_client.socket_fd, "Recv id from another client\n", 0);

        // recv name len
        ret = socketRecv(current_client.socket_fd, &current_client.name_len, 1);
        socketCheck(ret, current_client.socket_fd, "Recv name_len from another client\n", 0);

        // recv name
        ret = socketRecv(current_client.socket_fd, current_client.name, current_client.name_len);
        current_client.name[current_client.name_len] = '\0';
        socketCheck(ret, current_client.socket_fd, "Recv name from another client\n", 0);

        // recv ip len
        ret = socketRecv(current_client.socket_fd, &current_client.ip_len, 1);
        socketCheck(ret, current_client.socket_fd, "Recv ip_len from another client\n", 0);

        // recv ip
        ret = socketRecv(current_client.socket_fd, current_client.ip, current_client.ip_len);
        current_client.ip[current_client.ip_len] = '\0';
        socketCheck(ret, current_client.socket_fd, "Recv ip from another client\n", 0);

        // recv port len
        ret = socketRecv(current_client.socket_fd, &current_client.port_len, 1);
        socketCheck(ret, current_client.socket_fd, "Recv port_len from another client\n", 0);

        // recv port number
        ret = socketRecv(current_client.socket_fd, current_client.port, current_client.port_len);
        current_client.port[current_client.port_len] = '\0';
        socketCheck(ret, current_client.socket_fd, "Recv port from another client\n", 0);

        printflush("\n\033[A\rAccepting new client.\n"
                   "Neighbor [%d] Info: id: %d | (%d) %s | (%d) %s | (%d) port: %s\n$: ",
                   current_client.socket_fd,
                   current_client.id,
                   current_client.name_len,
                   current_client.name,
                   current_client.ip_len,
                   current_client.ip,
                   current_client.port_len,
                   current_client.port);

        pthread_t recv_pid;
        pthread_mutex_lock(&mutex);
        clients[peer_count] = current_client;
        peer_count++;

        struct receiveThreadPackage *receiveThreadPackage = malloc(sizeof(struct receiveThreadPackage));
        receiveThreadPackage->socket_fd = current_client.socket_fd;
        memcpy(receiveThreadPackage->name, current_client.name, NAMEMAX);
        pthread_create(&recv_pid, NULL, MSGReceiveWrapper, receiveThreadPackage);
        printflush("Creating thread for [socket %d] %s\n", receiveThreadPackage->socket_fd, receiveThreadPackage->name);

        listen_threads[listen_count] = recv_pid;
        listen_count++;
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}

void initialPrint(int argc, char *argv[], char *ip, char *server_ip) {
    printflush("Hello %s, you are at IP %s, and the SP IP is %s.\n",
               argv[2], ip, server_ip);
    printflush("Enter next message at the prompt or \\goodbye to quit. "
               "Type \\help for more commands.\n\n");
}

void socketCheck(int ret, int sockfd, char *msg, int mode) {
    if (ret == 0) {
        printflush("\n\033[A\rThe target socket %d has closed.", sockfd);
        printflush("%s\n$: ", msg);

        if (mode == 1) {
            close(sockfd);
            exit(0);
        } else if (mode == 2) {
            // remove this client from the clients array
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < peer_count; i++) {
                if (clients[i].socket_fd == sockfd) {
                    for (int j = i; j < peer_count - 1; j++) {
                        clients[j] = clients[j + 1];
                    }
                    peer_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            pthread_exit(NULL);
        }
    } else if (ret == -1) {
        printflush("The target socket %d has failed. The specific check message: ", sockfd);
        perror(msg);

        if (mode == 1) {
            close(sockfd);
            exit(1);
        } else if (mode == 2) {
            // remove this client from the clients array
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < peer_count; i++) {
                if (clients[i].socket_fd == sockfd) {
                    for (int j = i; j < peer_count - 1; j++) {
                        clients[j] = clients[j + 1];
                    }
                    peer_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            pthread_exit(NULL);
        }
    }
}

void getCurrentTimeString(char *buffer, tsize_t bufferSize) {
    time_t t;
    struct tm *timeptr;

    time(&t);
    timeptr = localtime(&t);

    strftime(buffer, bufferSize, "%Y%m%d-%H%M%S", timeptr); // Format: YYYYMMDD-HHMMSS
}

void createMSGID(tsize_t *ID, tsize_t ID_len, tsize_t *name) {
    char timeString[MSGMAX];
    getCurrentTimeString(timeString, sizeof(timeString));

    // concatenate time and name to create ID
    char *temp = malloc(sizeof(char) * MSGMAX);
    strcpy(temp, timeString);
    strcat(temp, "-");
    strcat(temp, (char *) name);
    tCast(temp, ID, ID_len);
    free(temp);
}

void processReceivedMSG(struct receiveMSGPackage receiveMSGPackage) {
    char *curr_ID = malloc(sizeof(char) * MSGMAX);
    strcpy(curr_ID, (char *) receiveMSGPackage.ID);

    int result = 0;
    pthread_mutex_lock(&mutex);
    result = checkIDFromLog(curr_ID);
    pthread_mutex_unlock(&mutex);
    if (result == 1) {
        free(curr_ID);
        return;
    }

    pthread_mutex_lock(&mutex);
    // print received ID and msg
    printflush("\n\033[A\rReceived message from socket %d: tag=%d len=%d msg=%s ID=%s hop=%d, "
               "PEER_COUNT = %d\n",
               receiveMSGPackage.socket_fd,
               receiveMSGPackage.msg_tag,
               receiveMSGPackage.msg_len,
               receiveMSGPackage.msg,
               receiveMSGPackage.ID,
               receiveMSGPackage.hop_count,
               peer_count);
    pthread_mutex_unlock(&mutex);

    char *dateString = strtok(curr_ID, "-");
    char *timeString = strtok(NULL, "-");
    char *name = strtok(NULL, "-");

    // concatenate dateString and timeString with a dash in between
    char *time = malloc(sizeof(char) * MSGMAX);
    strcpy(time, dateString);
    strcat(time, "-");
    strcat(time, timeString);

    // store the whole message
    char *msgString = malloc(sizeof(char) * MSGMAX);
    sprintf(msgString, "[%s] %s: %s | hops: %d", time, name, (char *) receiveMSGPackage.msg, receiveMSGPackage.hop_count);

    // set mutex to prevent two messages messing up together
    pthread_mutex_lock(&mutex);
    addMessageToLog(msgString, (char *) receiveMSGPackage.ID);
    printflush("\n\033[A\r[%s] %s: %s\n$: ", time, name, receiveMSGPackage.msg);
    receiveMSGPackage.hop_count++;
    forwardMSG(receiveMSGPackage);
    pthread_mutex_unlock(&mutex);

    free(curr_ID);
    free(time);
    free(msgString);
}

void processSendMSG(tsize_t *ID, tsize_t *msg) {
    char *curr_ID = malloc(sizeof(char) * MSGMAX);
    strcpy(curr_ID, (char *) ID);

    char *dateString = strtok(curr_ID, "-");
    char *timeString = strtok(NULL, "-");
    char *name = strtok(NULL, "-");

    // concatenate dateString and timeString with a dash in between
    char *time = malloc(sizeof(char) * MSGMAX);
    strcpy(time, dateString);
    strcat(time, "-");
    strcat(time, timeString);

    // store the whole message
    char *msgString = malloc(sizeof(char) * MSGMAX);
    sprintf(msgString, "[%s] %s: %s | Initial Sender", time, name, (char *) msg);
    pthread_mutex_lock(&mutex);
    addMessageToLog(msgString, (char *) ID);
    pthread_mutex_unlock(&mutex);

    free(curr_ID);
    free(time);
    free(msgString);
}

void addMessageToLog(const char *message, const char *ID) {
    if (log_count < MSGLOGMAX) {
        strncpy(MSGLOG[log_count], message, MAX_LOG_LENGTH);
        strncpy(MSGIDLOG[log_count], ID, MAX_LOG_LENGTH);
        MSGLOG[log_count][MAX_LOG_LENGTH - 1] = '\0';
        MSGIDLOG[log_count][MAX_LOG_LENGTH - 1] = '\0';
        log_count++;
    } else {
        viewLog();
        printflush("\n\033[A\rMessage log is full, clearing log\n$: ");
        clearLog();
    }
}

void clearLog() {
    for (int i = 0; i < MSGLOGMAX; i++) {
        MSGLOG[i][0] = '\0';
        MSGIDLOG[i][0] = '\0';
    }
    log_count = 0;
}

void viewLog() {
    // clear the terminal
//    printf("\033[H\033[J");
    printf("Message Log:\n");
    for (int i = 0; i < log_count; i++) {
        printf("%s\n", MSGLOG[i]);
    }
}

int checkIDFromLog(const char *ID) {
    // check if MSGIDLOG contains ID
    for (int i = 0; i < log_count; i++) {
        if (strcmp(MSGIDLOG[i], ID) == 0) {
            return 1;
        }
    }
    return 0;
}

void forwardMSG(struct receiveMSGPackage receiveMSGPackage) {
    int ret;

    // print everything in receiveMSGPackage in one print statement
    printflush("\n\033[A\rAbout to forward the received message from %d: tag=%d len=%d msg=%s ID=%s hop=%d\n$: ",
               receiveMSGPackage.socket_fd,
               receiveMSGPackage.msg_tag,
               receiveMSGPackage.msg_len,
               receiveMSGPackage.msg,
               receiveMSGPackage.ID,
               receiveMSGPackage.hop_count);

    // Send MSG
    for (int i = 0; i < peer_count; i++) {
        if (clients[i].socket_fd == receiveMSGPackage.socket_fd) continue;
//        sleep(1); // for demo purpose
        ret = MSGSend(clients[i].socket_fd, &receiveMSGPackage.msg_tag, &receiveMSGPackage.msg_len,
                      receiveMSGPackage.msg, &receiveMSGPackage.ID_len, receiveMSGPackage.ID, &receiveMSGPackage.hop_count);
        if (ret == 0) {
            printflush("Client %d closed in forward MSG\n", clients[i].socket_fd);
        } else if (ret == -1) {
            perror("Send msg failed in forward MSG\n");
        }
    }
}

void viewUserCommands() {
    printf("User Commands:\n");
    printf("  \\goodbye or \\exit or \\quit: quit the chat\n");
    printf("  \\log: view the message log\n");
    printf("  \\clear: clear the terminal\n");
}

void updateClientList(int socket_fd) {
//    printflush("Removing client with socket %d from the client list\n", socket_fd);
    for (int i = 0; i < peer_count; i++) {
        if (clients[i].socket_fd == socket_fd) {
            for (int j = i; j < peer_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            peer_count--;
            break;
        }
    }
}

void updateListenThreadList(pthread_t current_thread) {
//    printflush("Removing client with thread %ld from the listen thread list\n", current_thread);
    for (int i = 0; i < listen_count; i++) {
        if (pthread_equal(listen_threads[i], current_thread)) {
            for (int j = i; j < listen_count - 1; j++) {
                listen_threads[j] = listen_threads[j + 1];
            }
            listen_count--;
            break;
        }
    }
}

void *SPReceiveWrapper(void *arg) {
    struct SPPackage *spPackage;
    spPackage = (struct SPPackage *) arg;
    int sockfd = spPackage->client->socket_fd;
    int ret;

    while (1) {
        tsize_t tag = 0;
        ret = socketRecv(sockfd, &tag, 1);
        if (ret == 0) {
            printflush("\n\033[A\rThe connection to the SP has closed.\n$: ");
            close(sockfd);
            pthread_exit(NULL);
        } else if (ret == -1) {
            printflush("The connection to the SP has failed. The specific check message: ");
            perror("Receive msg failed\n");
            fflush(stderr);
            close(sockfd);
            pthread_exit(NULL);
        }

        if (tag == RECONNECT) {
            printflush("\n\033[A\rThe client is asked to relocate. Please wait...\n");

            // cancel all threads and reset all arrays
            pthread_mutex_lock(&mutex);
            reconnect = 1;
            tsize_t reconnect_tag = RECONNECT;
            ret = socketSend(sockfd, &reconnect_tag, 1);
            if (ret == -1) perror("Send reconnect tag to SP failed\n");
            close(sockfd);

            printflush("After closing socket %d\n", sockfd);

            tsize_t quit_tag = QUIT;
            // Close with other clients
            for (int i = 0; i < peer_count; i++) {
                ret = socketSend(clients[i].socket_fd, &quit_tag, 1);
            }
            peer_count = 0;
            printflush("After closing all peer sockets\n");
            printflush("peer_count: %d\n", peer_count);
            printflush("listen_count: %d\n", listen_count);
            printflush("After canceling all listen threads\n");

            reconnect = 0;
            pthread_cond_broadcast(&cond_var_main);
            pthread_mutex_unlock(&mutex);

            printflush("\n\033[A\rStart reconnection.\n");

            /****** Restart the whole process ******/
            struct sockaddr_in saddr;
            saddr.sin_port = htons(CS87_TALKSERVER_PORTNUM); // SP address setting
            saddr.sin_family = AF_INET;

            // Reinitialize the socket
            sockfd = socket(PF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("Failed to create socket");
                pthread_exit(NULL);
            }

            // Connect to the SP
            if (!inet_aton(spPackage->server_ip, &(saddr.sin_addr))) {
                perror("inet_aton");
                pthread_exit(NULL);
            }

            ret = connect(sockfd, (struct sockaddr *) &saddr, sizeof(saddr));
            if (ret < 0) {
                perror("Failed to reconnect to SP");
                pthread_exit(NULL);
            }

            // reassign socket
            spPackage->client->socket_fd = sockfd;

            /******************************************************************/
            // HELLO protocol
            helloCheck(spPackage->client);
            printflush("Client Connected to the SP. ");

            // Receive ID
            ret = socketRecv(sockfd, &spPackage->client->id, 1);
            socketCheck(ret, sockfd, "Recv ID from the SP\n", 1);
            printflush("Your Client ID: %d\n", spPackage->client->id);

            pthread_mutex_lock(&mutex);
            // Wait until listen_count and peer_count are both 0
            while (listen_count != 0 || peer_count != 0) {
                pthread_cond_wait(&cond_var_main, &mutex);
            }
            pthread_mutex_unlock(&mutex);

            // Receive list of other clients
            receiveClients(sockfd, spPackage->client);

//            // Create send thread
//            struct sendPackage sendPackage;
//            sendPackage.client = spPackage->client;
//            sendPackage.iters = iters;
//
//            pthread_create(&send_pid, NULL, MSGSendWrapper, &sendPackage);
        }
    }

    pthread_exit(NULL);
}

//static void special_cmd(int sockfd, char *msg) {
//    float prob = 0.5;
//    char *temp = malloc(sizeof(char) * 256);
//    if (strstr(msg, "\\speak") != NULL) {
//        if (sscanf(msg, "%s %f", temp, &prob) != 2)
//            prob = 1.0;
//        char *gibber = randomSpeak(10);
//        printflush("Command: speak, p = %f\n", prob);
//        if (gibber && ((float) rand() / RAND_MAX) < prob) {
//            printflush("%s\n", gibber);
//            // strcpy(msg, gibber);
//        }
//        free(gibber);
//    } else if (strstr(msg, "\\kill") != NULL) {
//        printflush("Kill command received, exitting\n");
//        if (sscanf(msg, "%s %f", temp, &prob) != 2)
//            prob = 1.0;
//        printflush("Command: kill, p = %f\n", prob);
//        if (((float) rand() / RAND_MAX) < prob) {
//            close(sockfd); // TODO: need something to exit nicely
//            free(temp);
//            raise(SIGTERM);
//        }
//        // exit(1);
//        // } else if (strstr(msg, "\\idkill") != NULL) {
//        //     // printf("Command: idkill\n");
//        // } else if (strstr(msg, "\\probkill") != NULL) {
//        //     // printf("Command: probkill\n");
//    }
//    free(temp);
//}

//static char *randomSpeak() {
//    int n = rand() % 10 + 1;
//    char filepath[128];
//    snprintf(filepath, sizeof(filepath), "/home/%s/cs87/Project-dzhen1-xdong1-kcao1/source/word.txt", getenv("USER"));
//    FILE *file = fopen(filepath, "r");
//    if (!file) {
//        perror("Error opening word file");
//        return NULL;
//    }
//    char words[1000][20];
//    int word_count = 0;
//
//    while (fscanf(file, "%s", words[word_count]) == 1) {
//        word_count++;
//    }
//
//    fclose(file);
//    char *result = malloc(20 * n);
//    result[0] = '\0';
//
//    for (int i = 0; i < n; i++) {
//        int random_index = rand() % word_count;
//        strcat(result, words[random_index]);
//        strcat(result, " ");
//    }
//    return result;
//}