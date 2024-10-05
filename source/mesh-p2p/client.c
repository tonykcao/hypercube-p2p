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
    struct sockaddr_in saddr, caddr, _addr;
    struct linger linger_val;

    // Initialize ip and port
    char port_self[MSGMAX];
    char *server_ip = argv[1];
    char *name = argv[2];
    char *ip_self = getIP();
    if (strcmp(server_ip, "0") == 0) server_ip = getIP();

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
    helloCheck(current);
    printflush("Client Connected to the SP. \n");

    // Receive list of other clients
    receiveClients(sockfd, &peer_count, current);

    struct sendPackage package;
    package.client = current;
    package.iters = iters;

    // Create send thread
    pthread_t send_pid;
    pthread_create(&send_pid, NULL, MSGSendWrapper, &package);

    // Listen to new clients
    addNewClientWrapper(listenfd, caddr, _addr);

    pthread_join(send_pid, NULL);

    printflush("bye, bye\n");
    close(sockfd);
}

void helloCheck(struct Client client) {
    int ret;
    tsize_t tag = HELLO_MSG;
    tsize_t reply_tag = 0;

    // send Hello tag
    ret = socketSend(client.socket_fd, &tag, 1);
    socketCheck(ret, client.socket_fd, "Send hello tag to the SP\n", 1);

    // send length of msg
    ret = socketSend(client.socket_fd, &client.name_len, 1);
    socketCheck(ret, client.socket_fd, "Send name_len to the SP\n", 1);

    // send name
    ret = socketSend(client.socket_fd, client.name, client.name_len);
    socketCheck(ret, client.socket_fd, "Send name to the SP\n", 1);

    // send length of ip
    ret = socketSend(client.socket_fd, &client.ip_len, 1);
    socketCheck(ret, client.socket_fd, "Send ip_len to the SP\n", 1);

    // send ip
    ret = socketSend(client.socket_fd, client.ip, client.ip_len);
    socketCheck(ret, client.socket_fd, "Send ip to the SP\n", 1);

    // send port len
    ret = socketSend(client.socket_fd, &client.port_len, 1);
    socketCheck(ret, client.socket_fd, "Send port_len to the SP\n", 1);

    // send port number
    ret = socketSend(client.socket_fd, client.port, client.port_len);
    socketCheck(ret, client.socket_fd, "Send port to the SP\n", 1);

    // recv reply tag
    ret = socketRecv(client.socket_fd, &reply_tag, 1);
    socketCheck(ret, client.socket_fd, "Recv reply tag from the SP\n", 1);

    // check reply tag
    if (reply_tag != HELLO_OK) {
        printflush("SP rejects the connection.\n bye, bye\n");
        close(client.socket_fd);
        exit(0);
    }
}

void *MSGReceiveWrapper(void *arg) {
    struct Client *client;
    client = (struct Client *) arg;

    int ret;
    int sockfd = client->socket_fd;
    tsize_t msg_tag, msg_len, ID_len;
    tsize_t msg[MSGMAX];
    tsize_t ID[MSGMAX];

    while (1) {
        // Receive MSG
        ret = MSGReceive(sockfd, &msg_tag, &msg_len, msg, &ID_len, ID);
        if (ret == -1) {
            printflush("\n\033[A\rThe connection to target [socket %d] %s has failed.\n$: ", sockfd, client->name);
            close(sockfd);

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

            pthread_exit(NULL);
        } else if (ret == 0) {
            printflush("\n\033[A\rThe target [socket %d] %s has closed.\n$: ", sockfd, client->name);
            close(sockfd);

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

            pthread_exit(NULL);
        }

        special_cmd(sockfd, (char *) msg);
        printReceivedMSG(ID, msg, 0);

        // Clear MSG
        memset(msg, 0, sizeof(msg));
        memset(ID, 0, sizeof(ID));
    }
}

void *MSGSendWrapper(void *arg) {
    struct sendPackage *send_args;
    send_args = (struct sendPackage *) arg;

    int ret;
    char *input = malloc(sizeof(char) * MSGMAX);
    size_t input_len;
    int sockfd = send_args->client.socket_fd;
    int iters = send_args->iters;

    int count = 0;
    int should_send = (iters != -1) ? (count <= iters) : 1;
    // char *gibber = NULL;
    while (should_send) {
        if (iters != -1) // CASE: testing
        {
            sleep(1);
            input = malloc(sizeof(char) * MSGMAX);
            // gibber = randomSpeak();
            // gibber ="";
            // sprintf(input, "[msg %d] %s", count, gibber);
            sprintf(input, "msg %d", count);
            // free(gibber);
            if (count == iters) {
                sprintf(input, "\\log");
            }
            count++;
            should_send = count <= iters;
        } else { // CASE: regular use
            input = readline("$: ");
            while (strlen(input) == 0) {
                free(input);
                input = readline(NULL);
            }
        }

        if (input == NULL) {
            close(sockfd);
            free(input);
            exit(1);
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
        createMSGID(ID_buffer, MSGMAX, send_args->client.name);
        tsize_t ID_len = sizeof(ID_buffer);

        if (strcmp(input, "\\goodbye") == 0 || strcmp(input, "\\quit") == 0 || strcmp(input, "\\exit") == 0) {
            tsize_t quit_tag = QUIT;
            ret = socketSend(sockfd, &quit_tag, 1);
            if (ret == -1) perror("Send quit tag to SP failed\n");
            close(sockfd);

            // Close with other clients
            for (int i = 0; i < peer_count; i++) {
                ret = socketSend(clients[i].socket_fd, &quit_tag, 1);
                close(clients[i].socket_fd);
            }
            printflush("Quit current client. Bye bye.\n");
            free(input);
            exit(0);
        }

        if (strcmp(input, "\\log") == 0) {
            viewLog();
            free(input);
            continue;
        }

        // initialize MSG Protocol
        tsize_t msg_tag = MSG_DATA;

        // Send MSG
        for (int i = 0; i < peer_count; i++) {
            ret = MSGSend(clients[i].socket_fd, &msg_tag, &msg_len, msg_buffer, &ID_len, ID_buffer);
            if (ret == 0) {
                printflush("Client %d closed.\n", clients[i].socket_fd);
                printflush("bye, bye\n");
                close(clients[i].socket_fd);

                // remove the client from the clients array
                pthread_mutex_lock(&mutex);
                for (int j = i; j < peer_count - 1; j++) {
                    clients[j] = clients[j + 1];
                }
                peer_count--;
                pthread_mutex_unlock(&mutex);
            } else if (ret == -1) {
                perror("Send msg failed\n");
                close(clients[i].socket_fd);

                // remove the client from the clients array
                pthread_mutex_lock(&mutex);
                for (int j = i; j < peer_count - 1; j++) {
                    clients[j] = clients[j + 1];
                }
                peer_count--;
                pthread_mutex_unlock(&mutex);
            }
        }

        // log the message by calling printReceivedMSG
        printReceivedMSG(ID_buffer, msg_buffer, 1);

        // Clear MSG
        memset(msg_buffer, 0, sizeof(msg_buffer));
        memset(ID_buffer, 0, sizeof(ID_buffer));
        free(input);
    }
    pthread_exit(NULL);
}

void receiveClients(int sockfd, tsize_t *peer_count, struct Client current) {
    int ret;
    struct sockaddr_in addr;

    // Receive number of clients
    ret = socketRecv(sockfd, peer_count, 1);
    socketCheck(ret, sockfd, "Recv number of other clients from the SP\n", 1);
    printflush("Number of other clients in the chat: %d\n", *peer_count);

    // Receive clients from SP
    for (int i = 0; i < *peer_count; i++) {
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

        printflush("Client [%d] info: (%d) %s | (%d) %s | (%d) port: %s\n",
                   i,
                   clients[i].name_len,
                   clients[i].name,
                   clients[i].ip_len,
                   clients[i].ip,
                   clients[i].port_len,
                   clients[i].port);

        /***** Connect to the current client *****/
        clients[i].socket_fd = socket(PF_INET, SOCK_STREAM, 0);
        addr.sin_port = htons(_atoi(clients[i].port));
        addr.sin_family = AF_INET;
        if (!inet_aton((char *) clients[i].ip, &(addr.sin_addr))) {
            perror("Connecting to one of other clients failed");
        }

        ret = connect(clients[i].socket_fd, (struct sockaddr *) &addr, sizeof(addr));
        socketErrorCheck(ret, clients[i].socket_fd, "Connect to a client failed\n");
//        printflush("Connecting to client %s, port: %d, sockfd: %d\n", clients[i].name, ntohs(addr.sin_port),
//                   clients[i].socket_fd);

        // send name len to client
        ret = socketSend(clients[i].socket_fd, &current.name_len, 1);
        socketCheck(ret, clients[i].socket_fd, "Send name_len to another client\n", 0);

        // send name to client
        ret = socketSend(clients[i].socket_fd, current.name, current.name_len);
        socketCheck(ret, clients[i].socket_fd, "Send name to another client\n", 0);

        // send ip len to client
        ret = socketSend(clients[i].socket_fd, &current.ip_len, 1);
        socketCheck(ret, clients[i].socket_fd, "Send ip_len to another client\n", 0);

        // send ip to client
        ret = socketSend(clients[i].socket_fd, current.ip, current.ip_len);
        socketCheck(ret, clients[i].socket_fd, "Send ip to another client\n", 0);

        // send port len to client
        ret = socketSend(clients[i].socket_fd, &current.port_len, 1);
        socketCheck(ret, clients[i].socket_fd, "Send port_len to another client\n", 0);

        // send port to client
        ret = socketSend(clients[i].socket_fd, current.port, current.port_len);
        socketCheck(ret, clients[i].socket_fd, "Send port to another client\n", 0);

        // create receiving thread for the client
        pthread_t recv_pid;
        pthread_create(&recv_pid, NULL, MSGReceiveWrapper, &(clients[i]));
        listen_threads[i] = recv_pid;
    }
}

void addNewClientWrapper(int listenfd, struct sockaddr_in caddr, struct sockaddr_in _addr) {
    int ret, confd = 0;
    unsigned int socklen = 0;

    while (1) {
//        printflush("listening on port %d\n", ntohs(caddr.sin_port));
        ret = listen(listenfd, BACKLOG);
        socketErrorCheck(ret, listenfd, "Listen failed\n");
        confd = accept(listenfd, (struct sockaddr *) &_addr, &socklen);
        if (confd == -1) {
            perror("failed at client accept");
            close(confd);
            continue;
        }

        struct Client current_client;
        current_client.socket_fd = confd;

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
        ret = socketRecv(current_client.socket_fd, current_client.port, (int) current_client.port_len);
        current_client.port[current_client.port_len] = '\0';
        socketCheck(ret, current_client.socket_fd, "Recv port from another client\n", 0);

        printflush("\n\033[A\rAccepting new client sockfd %d.\n"
                   "Info: (%d) %s | (%d) %s | (%d) port: %s\n$: ",
                   current_client.socket_fd,
                   current_client.name_len,
                   current_client.name,
                   current_client.ip_len,
                   current_client.ip,
                   current_client.port_len,
                   current_client.port);

        clients[peer_count] = current_client;

        // TODO: keep track of recv threads and join eventually
        pthread_t recv_pid;
        pthread_create(&recv_pid, NULL, MSGReceiveWrapper, &(clients[peer_count]));
        listen_threads[peer_count] = recv_pid;

        peer_count++;
    }
}

void initialPrint(int argc, char *argv[], char *ip, char *server_ip) {
    if (argc != 3) {
        printflush("Usage: ./cs87_client server_IP your_screen_name\n");
        exit(1);
    }
    printflush("Hello %s, you are at IP %s, trying to connect to talk server %s\n",
               argv[2], ip, server_ip);
    printflush("Enter next message at the prompt or goodbye to quit\n\n");
}

void socketCheck(int ret, int sockfd, char *msg, int mode) {
    if (ret == 0) {
        printflush("\n\033[A\rThe target socket %d has closed.", sockfd);
        printflush("%s\n$: ", msg);
        close(sockfd);

        if (mode == 1) {
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
            pthread_exit(NULL);
        }
    } else if (ret == -1) {
        printflush("The target socket %d has failed. The specific check message: ", sockfd);
        perror(msg);
        close(sockfd);

        if (mode == 1) {
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
            pthread_exit(NULL);
        }
    }
}

static char *randomSpeak() {
    int n = rand() % 10 + 1;
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/home/%s/cs87/Project-dzhen1-xdong1-kcao1/source/word.txt", getenv("USER"));
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Error opening word file");
        return NULL;
    }
    char words[1000][20];
    int word_count = 0;

    while (fscanf(file, "%s", words[word_count]) == 1) {
        word_count++;
    }

    fclose(file);
    char *result = malloc(20 * n);
    result[0] = '\0';

    for (int i = 0; i < n; i++) {
        int random_index = rand() % word_count;
        strcat(result, words[random_index]);
        strcat(result, " ");
    }
    return result;
}

static void special_cmd(int sockfd, char *msg) {
    float prob = 0.5;
    char *temp = malloc(sizeof(char) * 256);
    if (strstr(msg, "\\speak") != NULL) {
        if (sscanf(msg, "%s %f", temp, &prob) != 2)
            prob = 1.0;
        char *gibber = randomSpeak(10);
        printflush("Command: speak, p = %f\n", prob);
        if (gibber && ((float) rand() / RAND_MAX) < prob) {
            printflush("%s\n", gibber);
            // strcpy(msg, gibber);
        }
        free(gibber);
    } else if (strstr(msg, "\\kill") != NULL) {
        printflush("Kill command received, exitting\n");
        if (sscanf(msg, "%s %f", temp, &prob) != 2)
            prob = 1.0;
        printflush("Command: kill, p = %f\n", prob);
        if (((float) rand() / RAND_MAX) < prob) {
            close(sockfd); // TODO: need something to exit nicely
            free(temp);
            raise(SIGTERM);
        }
        // exit(1);
        // } else if (strstr(msg, "\\idkill") != NULL) {
        //     // printf("Command: idkill\n");
        // } else if (strstr(msg, "\\probkill") != NULL) {
        //     // printf("Command: probkill\n");
    }
    free(temp);
}

void getCurrentTimeString(char *buffer, tsize_t bufferSize) {
    time_t t;
    struct tm *timeptr;

    time(&t);
    timeptr = localtime(&t);

    strftime(buffer, bufferSize, "%Y%m%d-%H%M%S", timeptr); // Format: YYYYMMDD-HHMMSS
}

void createMSGID (tsize_t *ID, tsize_t ID_len, tsize_t *name) {
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

void printReceivedMSG (tsize_t *ID, tsize_t *msg, int mode) {
    char *temp = malloc(sizeof(char) * MSGMAX);
    strcpy(temp, (char *) ID);

    char *dateString = strtok(temp, "-");
    char *timeString = strtok(NULL, "-");
    char *name = strtok(NULL, "-");

    // concatenate dateString and timeString with a dash in between
    char *time = malloc(sizeof(char) * MSGMAX);
    strcpy(time, dateString);
    strcat(time, "-");
    strcat(time, timeString);

    // store the whole message
    char *msgString = malloc(sizeof(char) * MSGMAX);
    sprintf(msgString, "[%s] %s: %s", time, name, (char *) msg);
    addMessageToLog(msgString);

    // in mode 1, don't print the message (for logging send message)
    if (mode == 0) {
        // set mutex to prevent two messages messing up together
        pthread_mutex_lock(&mutex);
        printflush("\n\033[A\r[%s] %s: %s\n$: ", time, name, msg);
        pthread_mutex_unlock(&mutex);
    }

    free(temp);
    free(time);
    free(msgString);
}

void addMessageToLog(const char *message) {
    if (log_count < MSGLOGMAX) {
        strncpy(MSGLOG[log_count], message, MAX_LOG_LENGTH);
        MSGLOG[log_count][MAX_LOG_LENGTH - 1] = '\0';
        log_count++;
    } else {
        printflush("\n\033[A\rMessage log is full, clearing log\n$: ");
        clearLog();
    }
}

void clearLog() {
    for (int i = 0; i < MSGLOGMAX; i++) {
        MSGLOG[i][0] = '\0';
    }
    log_count = 0;
}

void viewLog() {
    // clear the terminal
    printf("\033[H\033[J");
    printf("Message Log:\n");
    for (int i = 0; i < log_count; i++) {
        printf("%s\n", MSGLOG[i]);
    }
}