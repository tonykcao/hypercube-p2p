#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#include <time.h>
#include "cs87talk.h"

void *MSGReceiveWrapper(void *arg);
void *MSGSendWrapper(void *arg);

struct t_args
{
    int sockfd;
    int iters;
};

int main(int argc, char **argv)
{
    int sockfd, ret;
    int iters = -1;           // not in testing mode
    struct sockaddr_in saddr; // server's IP & port number

    if (argc == 4)
    {
        iters = atoi(argv[3]);
        printflush("Testing mode, sending %d messages\n", iters);
    }
    else if (argc != 3)
    {
        printflush("Usage: ./cs87_client server_IP your_screen_name\n");
        exit(1);
    }
    printflush("Hello %s, you are trying to connect to talk server %s\n",
               argv[2], argv[1]);
    printflush("enter next message at the prompt or goodbye to quit\n\n");

    // step (1) create a TCP socket
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("Create Socket Failed\n");
        exit(1);
    }

    // initialize the saddr struct that you will pass to connect in step (2):
    // (***this part is complete, you don't need to change it)
    // server's IP is in argv[1] (a string like "130.58.68.129")
    //  inet_pton converts IP from dotted decimal formation
    //  to 32-bit binary address (inet_pton coverts the other way)
    saddr.sin_port = htons(CS87_TALKSERVER_PORTNUM);
    saddr.sin_family = AF_INET;
    if (!inet_aton(argv[1], &(saddr.sin_addr)))
    {
        perror("inet_aton");
        exit(1);
    }

    char *name = argv[2];
    if (strlen(name) > NAMEMAX)
    {
        name[NAMEMAX] = '\0';
    }

    tsize_t t_msg[NAMEMAX];

    t_cast(name, t_msg, NAMEMAX);

    // Connect to talk server
    ret = connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret == -1)
    {
        perror("Connect Failed\n");
        close(sockfd);
        exit(1);
    }

    // Initialize HELLO protocol
    tsize_t tag = HELLO_MSG;
    tsize_t len = strlen(name);
    tsize_t reply_tag;
    ret = send_msg(sockfd, &tag, 1);
    if (ret == -1)
    {
        perror("Send tag Failed\n");
        close(sockfd);
        exit(1);
    }
    else if (ret == 0)
    {
        perror("Server closed\n");
        close(sockfd);
        exit(0);
    }

    ret = send_msg(sockfd, &len, 1);
    if (ret == -1)
    {
        perror("Send length Failed\n");
        close(sockfd);
        exit(1);
    }
    else if (ret == 0)
    {
        perror("Server closed\n");
        close(sockfd);
        exit(0);
    }

    ret = send_msg(sockfd, t_msg, len);
    if (ret == -1)
    {
        perror("Send name Failed\n");
        close(sockfd);
        exit(1);
    }
    else if (ret == 0)
    {
        perror("Server closed\n");
        close(sockfd);
        exit(0);
    }

    // Receive HELLO reply
    ret = recv(sockfd, &reply_tag, sizeof(reply_tag), 0);

    struct t_args *send_thread_args = malloc(sizeof(struct t_args) * 1);
    send_thread_args->sockfd = sockfd;
    send_thread_args->iters = iters;
    printf("sockfd: %d, iters: %d\n", send_thread_args->sockfd, send_thread_args->iters);

    if (reply_tag == HELLO_OK)
    {
        printflush("Connected!\n");

        // Initialize threads
        pthread_t recv_pid;
        pthread_t send_pid;

        // Create threads
        pthread_create(&recv_pid, NULL, MSGReceiveWrapper, &sockfd);
        pthread_create(&send_pid, NULL, MSGSendWrapper, (void *)send_thread_args);

        pthread_join(recv_pid, NULL);
        pthread_join(send_pid, NULL);

        printflush("bye, bye\n");
        close(sockfd);
    }
    else
    {
        printflush("Server rejects the connection.\n");
        printflush("bye, bye\n");
        close(sockfd);
    }
}

/**
 * Wrapper function for MSGReceive (passed to thread)
 * @param arg a pointer to the socket file descriptor
 * @return
 */
void *MSGReceiveWrapper(void *arg)
{
    int ret;
    int sockfd = *((int *)arg);
    tsize_t msg_tag, msg_len;
    tsize_t msg[MSGMAX];

    time_t timer;
    char buffer[9];
    struct tm *tm_info;
    timer = time(NULL);

    while (1)
    {
        // Receive MSG
        ret = MSGReceive(sockfd, &msg_tag, &msg_len, msg);
        if (ret == 0)
        {
            printflush("Server closed.\n");
            printflush("bye, bye\n");
            close(sockfd);
            exit(0);
        }
        else if (ret == -1)
        {
            perror("Recv msg failed\n");
            printflush("bye, bye\n");
            close(sockfd);
            exit(1);
        }
        else
        {
            timer = time(NULL);
            tm_info = localtime(&timer);
            strftime(buffer, 9, "%H:%M:%S", tm_info);
            printflush("%s | %s\n", buffer, msg);
        }

        // Clear MSG
        memset(msg, 0, sizeof(msg));
    }
}

/**
 * Wrapper function for MSGSend (passed to thread)
 * @param arg a pointer to the socket file descriptor
 * @return
 */
void *MSGSendWrapper(void *args)
{
    int count = 0;
    struct t_args *send_args = (struct t_args *)args;
    int sockfd = send_args->sockfd;
    int iters = send_args->iters;
    int should_send = (iters!=-1) ? (count<iters) : 1;

    while (should_send)
    {
        char *input = malloc(sizeof(char) * MSGMAX);
        size_t input_len;

        if (iters != -1) // CASE: testing
        {
            sleep(1);
            sprintf(input, "test message %d", count);
            if (count == iters)
            {
                sprintf(input, "goodbye");
            }
            count++;
            should_send = count<iters;
        }
        else
        { // CASE: regular use
            input = readline("$: ");
            while (strlen(input) == 0)
            {
                free(input);
                input = readline("$: ");
            }
        }

        if (input == NULL)
        {
            close(sockfd);
            free(input);
            exit(1);
        }

        input_len = strlen(input);

        if (input_len > MSGMAX)
        {
            input[MSGMAX] = '\0';
            input_len = MSGMAX;
        }

        tsize_t t_msg[MSGMAX];
        tsize_t msg_len = input_len;
        t_cast(input, t_msg, MSGMAX);

        int ret = strcmp(input, "goodbye");

        if (ret == 0)
        {
            // initialize QUIT Protocol
            tsize_t quit_tag = QUIT;
            ret = send_msg(sockfd, &quit_tag, 1);
            if (ret == 1)
            {
                close(sockfd);
                free(input);
                exit(0);
            }
        }
        else
        {
            // initialize MSG Protocol
            tsize_t msg_tag = MSG_DATA;

            // Send MSG
            ret = MSGSend(sockfd, &msg_tag, &msg_len, t_msg);
            if (ret == 0)
            {
                printflush("Server closed.\n");
                printflush("bye, bye\n");
                close(sockfd);
                free(input);
                exit(0);
            }
            else if (ret == -1)
            {
                perror("Send msg failed\n");
                printflush("bye, bye\n");
                close(sockfd);
                free(input);
                exit(1);
            }

            // Clear MSG
            memset(t_msg, 0, sizeof(t_msg));
        }
        free(input);
    }

    return NULL;
}