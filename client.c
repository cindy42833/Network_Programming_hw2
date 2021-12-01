#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

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
    char input_acc[64], input_pwd[64] , buf[1024];

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
            if(strcmp(buf, "Login Success") == 0) {
                printf("Login Success\n");
                return 0;
            }
            else if(strcmp(buf, "Login Fail") == 0) {
                if(i > 0)
                    printf("Login Fail, you have %d times to try\n", i);
                else
                    printf("Login Fail, connection closed\n");
            }
            else if(strcmp(buf, "Not allow to repeatly login") == 0)
                printf("Not allow to repeatly login\n");
        }
    }
    return -1;
}

int main()
{
    int socketfd, readCnt = 0;
    char input_acc[64], input_pwd[64], buf[1024];

    socketfd = connect_to_server("127.0.0.1", "8080");

    if(Login(socketfd) < 0) {
        exit(EXIT_SUCCESS);
    }
    while(fgets(buf, 1024, stdin) != NULL) {
        ;
    }
}