#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

#define Buffer_Max 1024
fd_set fds, read_fds;

int connect_to_server(const char *host, const char *port)
{

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;       // AF_INET is IPv4
    hints.ai_socktype = SOCK_STREAM; // use TCP

    struct addrinfo *host_addr;
    getaddrinfo(host, port, &hints, &host_addr); // if host is not NULL, AI_PASSIVE flag is ignored

    printf("Creating socket...\n");
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd < 0)
    {
        perror("Create socket error");
        exit(EXIT_FAILURE);
    }

    if (connect(socketfd, host_addr->ai_addr, host_addr->ai_addrlen) == -1)
    {
        perror("Connect error");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(host_addr);

    return socketfd;
}

int Login(int socketfd)
{
    int readCnt = 0;
    char input_acc[64], input_pwd[64], buf[1024];

    for (int i = 2; i >= 0; i--)
    {
        printf("acc: ");
        scanf("%s", input_acc);
        printf("pwd: ");
        scanf("%s", input_pwd);

        if (write(socketfd, input_acc, sizeof(input_acc)) < 0)
            perror("Client write error");
        if (write(socketfd, input_pwd, sizeof(input_pwd)) < 0)
            perror("Client write error");

        if ((readCnt = read(socketfd, buf, sizeof(buf))) <= 0)
        {
            if (readCnt == 0)
            {
                printf("Server has terminated\n");
                exit(EXIT_SUCCESS);
            }
            else
            {
                perror("Client write error");
                continue;
            }
        }
        else
        {
            if (strcmp(buf, "Login Success") == 0)
            {
                printf("Login Success\n");
                return 0;
            }
            else if (strcmp(buf, "Login Fail") == 0)
            {
                if (i > 0)
                    printf("Login Fail, you have %d times to try\n", i);
                else
                    printf("Login Fail, connection closed\n");
            }
            else if (strcmp(buf, "Not allow to repeatly login") == 0)
                printf("Not allow to repeatly login\n");
        }
    }
    return -1;
}

int sendRequest(int socketfd)
{
    int readCnt = 0, writeCnt = 0;
    char buf[Buffer_Max];

    if (fgets(buf, 1024, stdin) == NULL)
        return -1;

    buf[strlen(buf) - 1] = '\0';
    if(strncmp(buf, "logout", 6) == 0)
        return 0;

    if (write(socketfd, buf, sizeof(buf)) < 0)
    {
        perror("Client write error");
        return -1;
    }
    
    return 1;
}

int parseResponse(int socketfd) {
    int readCnt = 0;
    char buf[Buffer_Max];

    buf[strlen(buf) - 1] = '\0';

    readCnt = read(socketfd, buf, sizeof(buf));
    if (readCnt == 0)
    {
        printf("Server has terminated\n");
        return 0;
    }
    else if (readCnt < 0)
    {
        perror("Client read error");
        return -1;
    }
    else
    {
        printf("%s\n", buf);
        return 1;
    }
}

int main()
{
    int socketfd, fdmax, ret = 0;

    socketfd = connect_to_server("127.0.0.1", "8080");
    if (Login(socketfd) < 0)
        exit(EXIT_SUCCESS);

    FD_ZERO(&fds);
    FD_ZERO(&read_fds);
    FD_SET(0, &fds); // stdin
    FD_SET(socketfd, &fds);
    fdmax = socketfd;
    getchar();
    while (1)
    {
        read_fds = fds;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("Select error");
            continue;
        }

        if (FD_ISSET(socketfd, &read_fds))
            ret = parseResponse(socketfd);
        else
            ret = sendRequest(socketfd);
            
        if(ret == -1)
            continue;
        else if(ret == 0)
            break;
    }
    shutdown(socketfd, SHUT_WR);
    close(socketfd);
    return 0;
}