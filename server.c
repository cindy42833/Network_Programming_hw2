#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

#define USER_MAX 4

typedef struct user
{
    int fd;
    int online;
    char acc[64];
    char pwd[64];
} Users;

Users users[] = {
    {.fd = -1,
     .online = 0,
     .acc = "aaa",
     .pwd = "aaa"},
    {.fd = -1,
     .online = 0,
     .acc = "bbb",
     .pwd = "bbb"},
    {.fd = -1,
     .online = 0,
     .acc = "ccc",
     .pwd = "ccc"},
    {.fd = -1,
     .online = 0,
     .acc = "ddd",
     .pwd = "ddd"},
};

int fdmax, old_fdmax;
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

int userLogin(int connectFd)
{
    int readCnt = 0;
    char buf[1024];

    char input_acc[64], input_pwd[64];

    if ((readCnt = read(connectFd, input_acc, sizeof(input_acc))) <= 0)
    {
        if (readCnt == 0)
        { // client terminate
            printf("Client disconnection\n");
        }
        else
            perror("Server read error");
        return -1;
    }
    if ((readCnt = read(connectFd, input_pwd, sizeof(input_pwd))) <= 0)
        perror("Server read error");

    for (int j = 0; j < USER_MAX; j++)
    {
        if (strcmp(users[j].acc, input_acc) == 0 && strcmp(users[j].pwd, input_pwd) == 0)
        {
            if (users[j].online)
            {
                sprintf(buf, "Not allow to repeatly login");
                if (write(connectFd, buf, sizeof(buf)) < 0)
                    perror("Server write error");
                return 0;
            }
            else
            {
                users[j].fd = connectFd; // record a user fd
                users[j].online = 1;
                sprintf(buf, "Login Success");

                if (write(connectFd, buf, sizeof(buf)) < 0)
                    perror("Server write error");
                return j;
            }
        }
    }
    sprintf(buf, "Login Fail");
    if (write(connectFd, buf, sizeof(buf)) < 0)
        perror("Server write error");

    return 0;
}

int main()
{
    int listenFd, connectFd;
    int userlist[64];
    char buf[1024];
    memset(userlist, -1, sizeof(userlist));

    listenFd = create_socket(NULL, "8080");

    FD_ZERO(&fds);
    FD_ZERO(&read_fds);
    FD_SET(listenFd, &fds);
    fdmax = listenFd;
    old_fdmax = fdmax;

    while (1)
    {
        read_fds = fds;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenFd, &read_fds))
        {
            if ((connectFd = accept(listenFd, NULL, NULL)) < 0)
            {
                perror("Accept error");
                continue;
            }
            else
            {
                if (connectFd > fdmax)
                {
                    old_fdmax = fdmax;
                    fdmax = connectFd;
                }
                FD_SET(connectFd, &fds);
            }
        }
        else
        {
            for (int i = 0; i <= fdmax; i++)
            {
                if (FD_ISSET(i, &read_fds))
                {
                    printf("list: %d\n", userlist[i]);
                    if (userlist[i] == -1)
                    { // login
                        int login = 0;
                        if ((login = userLogin(i)) < 0)
                        {
                            fdmax = old_fdmax;
                            FD_CLR(i, &fds);
                        }
                        else if (login > 0)
                            userlist[i] = login;
                    }
                    // else
                    // {
                    //     /* Do other */
                    // }
                    break;
                }
            }
        }
    }
    return 0;
}