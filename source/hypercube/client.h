#ifndef _CLIENT_H_
#define _CLIENT_H_

static struct Client clients[MAX_CLIENTS];
static tsize_t peer_count = 0;

static char MSGLOG[MSGLOGMAX][MAX_LOG_LENGTH];
static char MSGIDLOG[MSGLOGMAX][MAX_LOG_LENGTH];
static int log_count = 0;

pthread_t listen_threads[MAX_CLIENTS];
static int listen_count = 0;

static pthread_mutex_t mutex;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_var_main = PTHREAD_COND_INITIALIZER;
int ready_to_start = 0;
int ready_to_quit = 0;
int waiting_threads = 0;
int reconnect = 0;

static pthread_t send_pid;
static pthread_t listen_pid;
static pthread_t sp_pid;

struct sendPackage {
    struct Client *client;
    int iters;
};

struct listenPackage {
    int listenfd;
};

struct receiveMSGPackage {
    int socket_fd;
    tsize_t msg_tag;
    tsize_t msg_len;
    tsize_t ID_len;
    tsize_t msg[MSGMAX];
    tsize_t ID[MSGMAX];
    tsize_t hop_count;
};

struct receiveThreadPackage {
    int socket_fd;
    tsize_t name[NAMEMAX];
};

struct SPPackage {
    struct Client *client;
    char *server_ip;
    int iters;
};

/**
 * Wrapper function for initial hello protocol check.
 * @param client the client struct
 */
void helloCheck(struct Client *client);

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
void receiveClients(int sockfd, struct Client *client);

/**
 * Wrapper function for adding a new client to the chat
 * @param listenfd the listening socket file descriptor
 * @param caddr the client address
 * @param _addr the server address
 */
void *addNewClientWrapper(void *arg);

void *SPReceiveWrapper(void *arg);

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

// static char* randomSpeak();

// static void special_cmd(int sockfd, char* msg);

void getCurrentTimeString(char *buffer, tsize_t bufferSize);

void createMSGID(tsize_t *ID, tsize_t ID_len, tsize_t *name);

void processReceivedMSG(struct receiveMSGPackage receiveMSGPackage);

void processSendMSG(tsize_t *ID, tsize_t *msg);

void addMessageToLog(const char *message, const char *msgID);

void clearLog();

void viewLog();

int checkIDFromLog(const char *ID);

void forwardMSG(struct receiveMSGPackage receiveMSGPackage);

void viewUserCommands();

void updateClientList(int sockfd);

void updateListenThreadList(pthread_t thread_id);

#endif
