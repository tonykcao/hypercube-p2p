#ifndef _SERVER_H_
#define _SERVER_H_


int helloCheck(int confd, tsize_t *tag, tsize_t *len, tsize_t *name, tsize_t *ip_len, tsize_t *ip, tsize_t *port_len,tsize_t *port);

void addClient(struct Client *newClient);

void removeClient(int socket_fd);

void *CommunicationWrapper(void *arg);

void broadcast(tsize_t *msg, tsize_t len);

#endif
