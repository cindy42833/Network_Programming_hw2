#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

#define USER_MAX 4
#define Buffer_Max 1024

typedef struct user
{
    int fd;
    int online;
    int onGame;
    char acc[64];
    char pwd[64];
} Users;

Users users[] = {
    {.fd = -1,
     .online = 0,
     .onGame = 0,
     .acc = "aaa",
     .pwd = "aaa"},
    {.fd = -1,
     .online = 0,
     .onGame = 0,
     .acc = "bbb",
     .pwd = "bbb"},
    {.fd = -1,
     .online = 0,
     .onGame = 0,
     .acc = "ccc",
     .pwd = "ccc"},
    {.fd = -1,
     .online = 0,
     .onGame = 0,
     .acc = "ddd",
     .pwd = "ddd"},
};

int fdmax;
fd_set fds, read_fds;

int create_socket(const char *host, const char *port)
{
    printf("Configuring local address...\n");

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;       // AF_INET is IPv4
    hints.ai_socktype = SOCK_STREAM; // use TCP
    hints.ai_flags = AI_PASSIVE;     // listening socket, fill in host IP for me

    struct addrinfo *bind_addr;
    getaddrinfo(host, port, &hints, &bind_addr); // if host is not NULL, AI_PASSIVE flag is ignored

    printf("Creating socket...\n");
    int socket_listen = socket(bind_addr->ai_family, bind_addr->ai_socktype, bind_addr->ai_protocol);

    if (socket_listen < 0)
    {
        perror("Create socket error");
        exit(EXIT_FAILURE);
    }
    int enable = 1;
    setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_addr->ai_addr, bind_addr->ai_addrlen) == -1)
    {
        perror("Bind socket error");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(bind_addr);
    printf("Listening...\n");
    int waitingQueue = 10; // defines the maximum length of the queue
    if (listen(socket_listen, waitingQueue) == -1)
    {
        perror("Listen socket error");
        exit(EXIT_FAILURE);
    }

    return socket_listen;
}

void acceptClient(int listenFd)
{
    int connectFd;
    if ((connectFd = accept(listenFd, NULL, NULL)) < 0)
        perror("Accept error");
    else
    {
        if (connectFd > fdmax)
            fdmax = connectFd;
        FD_SET(connectFd, &fds);
    }
}

void userLogin(int connectFd, int *userlist)
{
    int readCnt = 0;
    char buf[Buffer_Max];

    char input_acc[64], input_pwd[64];

    if ((readCnt = read(connectFd, input_acc, sizeof(input_acc))) <= 0)
    {
        if (readCnt == 0)
        { // client terminate
            printf("Client disconnection\n");
            FD_CLR(connectFd, &fds);
            close(connectFd);
        }
        else
            perror("Server read error");
    }
    else
    {
        if ((readCnt = read(connectFd, input_pwd, sizeof(input_pwd))) <= 0)
            perror("Server read error");
        else
        {
            for (int j = 0; j < USER_MAX; j++)
            {
                if (strcmp(users[j].acc, input_acc) == 0 && strcmp(users[j].pwd, input_pwd) == 0)
                {
                    if (users[j].online) // Login repeatly
                    {
                        sprintf(buf, "Not allow to repeatly login");
                        if (write(connectFd, buf, sizeof(buf)) < 0)
                            perror("Server write error");
                    }
                    else // Login success
                    {
                        users[j].fd = connectFd; // record a user fd
                        users[j].online = 1;
                        *userlist = j; // record user id
                        sprintf(buf, "Login Success");

                        if (write(connectFd, buf, sizeof(buf)) < 0)
                            perror("Server write error");
                    }
                    return;
                }
            }
            // Login fail
            sprintf(buf, "Login Fail");
            if (write(connectFd, buf, sizeof(buf)) < 0)
                perror("Server write error");
        }
    }
}

void userLogout(int connectFd, int *userlist) {
    char buf[Buffer_Max];

    printf("Client logout\n");
    users[userlist[connectFd]].fd = -1;
    users[userlist[connectFd]].online = 0;
    users[userlist[connectFd]].onGame = 0;

    userlist[connectFd] = -1;
    sprintf(buf, "logout");
    if (write(connectFd, buf, sizeof(buf)) < 0)
        perror("Server write error");

    FD_CLR(connectFd, &fds);
    shutdown(connectFd, SHUT_RD);    
    close(connectFd);
}

void listUsers(int connectFd) {
    char buf[Buffer_Max], info[128];

    sprintf(buf, "------- Online list -------\n");

    for(int i=0; i<USER_MAX; i++) {
        if(users[i].online) {
            sprintf(info, "userId: %d, acc: %s\n", i, users[i].acc);
            strncat(buf, info, strlen(info));
        }
    }
    sprintf(info, "-------- End list  --------\n");
    strncat(buf, info, strlen(info));
    if (write(connectFd, buf, sizeof(buf)) < 0)
        perror("Server write error");
}

void sentInvitation(int *userlist, int selfFd, int opponentFd) {
    char buf[Buffer_Max];

    sprintf(buf, "%s wants to play with you, type yes or no: \n", users[userlist[selfFd]].acc);
    if (write(opponentFd, buf, sizeof(buf)) < 0) {
        perror("Server write error");
        sprintf(buf, "Send Error");
        if (write(selfFd, buf, sizeof(buf)) < 0)
            perror("Server write error");
        return;
    }
    sprintf(buf, "Waiting...\n");
    if(write(selfFd, buf, sizeof(buf)) < 0) {
        perror("Server write error");
    }
    return;
}

int main()
{
    int listenFd, connectFd, readCnt, writeCnt, opponent;
    int userlist[64];
    char buf[Buffer_Max];
    memset(userlist, -1, sizeof(userlist));


    listenFd = create_socket(NULL, "8080");

    FD_ZERO(&fds);
    FD_ZERO(&read_fds);
    FD_SET(listenFd, &fds);
    fdmax = listenFd;

    while (1)
    {
        read_fds = fds;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenFd, &read_fds))
            acceptClient(listenFd);
        else
        {
            for (int i = 0; i <= fdmax; i++)
            {
                if (FD_ISSET(i, &read_fds))
                {
                    if (userlist[i] == -1) // Login
                        userLogin(i, &userlist[i]);
                    else
                    {
                        if ((readCnt = read(i, buf, sizeof(buf))) <= 0)
                        {
                            if (readCnt == 0)
                            {
                                printf("Client disconnection\n");
                                users[userlist[i]].online = 0;
                                FD_CLR(i, &fds);
                                close(i);
                            }
                            else
                                perror("Server read error");
                        }
                        else
                        {
                            if (strncmp(buf, "list", 4) == 0)
                                listUsers(i);
                            else if(strncmp(buf, "logout", 6) == 0) {
                                userLogout(i, userlist);
                            }
                            else if(strncmp(buf, "PK", 2) == 0){
                                opponent = atoi(buf + 3);
                                printf("opponent: %d, fd: %d\n", opponent, users[opponent].fd);
                                sentInvitation(userlist, i, users[opponent].fd);
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    return 0;
}