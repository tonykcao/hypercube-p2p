#ifndef _SERVER_H_
#define _SERVER_H_

static struct Client clientMap[MAX_CLIENTS];
static tsize_t minID = 0;
static tsize_t maxID = 0;
static int num_clients;

static pthread_mutex_t mutex;

int helloCheck(struct Client *client);

void addClient(struct Client *client);

void removeClient(tsize_t id);

void *clientWrapper(void *arg);

void sendClients(struct Client *current, tsize_t *neighbors);

/**************************************************************/
/********************* Helper Functions ***********************/
/**************************************************************/


void socketCheck(int ret, int sockfd, char *msg, int mode, int id);

void initializeMap();

void printNeighbors(tsize_t *neighbors);

void repairTopology(tsize_t id);

void updateMINID();

void updateMAXID();

#endif
