#ifndef _CLIENT_H_
#define _CLIENT_H_

static struct Client clients[MAX_CLIENTS];
static tsize_t peer_count = 0;
static char MSGLOG[MSGLOGMAX][MAX_LOG_LENGTH];
static int log_count = 0;
static pthread_mutex_t mutex;
pthread_t listen_threads[MAX_CLIENTS];

struct sendPackage {
    struct Client client;
    int iters;
};

/**
 * Wrapper function for initial hello protocol check.
 * @param client the client struct
 */
void helloCheck(struct Client client);

/**
 * Wrapper function for MSGReceive (passed to thread)
 * @param arg a pointer to the socket file descriptor
 */
void *MSGReceiveWrapper(void *arg);

/**
 * Wrapper function for MSGSend (passed to thread)
 * @param arg a pointer to the socket file descriptor
 */
void *MSGSendWrapper(void *arg);

/**
 * Wrapper function for receiving OTHER clients in the chat and
 * creating a new thread for each client
 * @param sockfd socket file descriptor
 * @param num_clients number of clients
 */
void receiveClients(int sockfd, tsize_t *num_clients, struct Client client);

/**
 * Wrapper function for adding a new client to the chat
 * @param listenfd the listening socket file descriptor
 * @param caddr the client address
 * @param _addr the server address
 */
void addNewClientWrapper(int listenfd, struct sockaddr_in caddr, struct sockaddr_in _addr);

/**************************************************************/
/********************* Helper Functions ***********************/
/**************************************************************/

/**
 * Prints the initial message
 * @param argc argument count
 * @param argv argument vector
 * @param ip ip address
 * @param server_ip server ip address
 */
void initialPrint(int argc, char *argv[], char *ip, char *server_ip);

void socketCheck(int ret, int sockfd, char *msg, int mode);

static char* randomSpeak();

static void special_cmd(int sockfd, char* msg);

void getCurrentTimeString(char *buffer, tsize_t bufferSize);

void createMSGID(tsize_t *ID, tsize_t ID_len, tsize_t *name);

void printReceivedMSG(tsize_t *ID, tsize_t *msg, int mode);

void addMessageToLog(const char *message);

void clearLog();

void viewLog();

#endif
