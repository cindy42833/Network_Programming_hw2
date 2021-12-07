#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>

#define USER_MAX 4
#define ROOM_MAX 4
#define Buffer_Max 1024

typedef struct user
{
    int fd;
    int online;
    int onGame;
    int roomID;
    char acc[64];
    char pwd[64];
} Users;

typedef struct room
{
    char board[9];
    int player1;
    int player2;
    int round;
} Rooms;

Users users[] = {
    {.fd = -1,
     .online = 0,
     .onGame = -1,
     .roomID = -1,
     .acc = "aaa",
     .pwd = "aaa"},
    {.fd = -1,
     .online = 0,
     .onGame = -1,
     .roomID = -1,
     .acc = "bbb",
     .pwd = "bbb"},
    {.fd = -1,
     .online = 0,
     .onGame = -1,
     .roomID = -1,
     .acc = "ccc",
     .pwd = "ccc"},
    {.fd = -1,
     .online = 0,
     .onGame = -1,
     .roomID = -1,
     .acc = "ddd",
     .pwd = "ddd"},
};

Rooms rooms[4] = {
    {
        .board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .player1 = -1,
        .player2 = -1,
        .round = 0
    },
    {
        .board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .player1 = -1,
        .player2 = -1,
        .round = 0
    },
    {
        .board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .player1 = -1,
        .player2 = -1,
        .round = 0
    },
    {
        .board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .player1 = -1,
        .player2 = -1,
        .round = 0
    },
};

int RoomActive[4], userlist[64];
int fdmax, errno;
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

void userLogout(int connectFd)
{
    int id = userlist[connectFd];
    users[id].fd = -1;
    users[id].online = 0;
    users[id].onGame = -1;
    users[id].roomID = -1;

    userlist[connectFd] = -1;
    FD_CLR(connectFd, &fds);
    shutdown(connectFd, SHUT_RD);
    close(connectFd);
}

int Read(int fd, char *buf, int buf_size)
{
    int readCnt = -1;

    if (fd >= 0)
    {
        if ((readCnt = read(fd, buf, buf_size)) == 0)
        {
            // client terminate
            printf("Client disconnection\n");
            if (userlist[fd] != -1)
                userLogout(fd);
            else
            {
                FD_CLR(fd, &fds);
                shutdown(fd, SHUT_RD);
                close(fd);
            }
        }
        else if (readCnt < 0)
            perror("Server read error");
    }
    return readCnt;
}

int Write(int fd, char *buf, int buf_size)
{
    int writeCnt = -1;

    if (fd >= 0)
    {
        if ((writeCnt = write(fd, buf, buf_size)) < 0)
        {
            if (errno == SIGPIPE)
            {
                printf("Client disconnection\n");
                if (userlist[fd] != -1)
                    userLogout(fd);
                else
                {
                    FD_CLR(fd, &fds);
                    shutdown(fd, SHUT_RD);
                    close(fd);
                }
            }
            perror("Server write error");
        }
    }
    return writeCnt;
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

void userLogin(int connectFd)
{
    int readCnt = 0;
    char buf[Buffer_Max];

    char input_acc[64], input_pwd[64];

    if (Read(connectFd, input_acc, sizeof(input_acc)) > 0)
    {
        if (Read(connectFd, input_pwd, sizeof(input_pwd)) > 0)
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
                        userlist[connectFd] = j; // record user id
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

void listUsers(int connectFd)
{
    char buf[Buffer_Max], info[128];

    sprintf(buf, "------- Online list -------\n");

    for (int i = 0; i < USER_MAX; i++)
    {
        if (users[i].online)
        {
            sprintf(info, "userId: %d, acc: %s\n", i, users[i].acc);
            strncat(buf, info, strlen(info));
        }
    }
    sprintf(info, "-------- End list  --------\n");
    strncat(buf, info, strlen(info));
    if (write(connectFd, buf, sizeof(buf)) < 0)
        perror("Server write error");
}

int selectRoom()
{
    for (int i = 0; i < ROOM_MAX; i++)
    {
        if (RoomActive[i] == 0)
            return i;
    }
    return -1;
}

void leaveGame(int userID)
{
    users[userID].onGame = -1;
    users[userID].roomID = -1;
}

void resetGame(int roomID)
{
    RoomActive[roomID] = 0;
    memset(rooms[roomID].board, ' ', 9);
    rooms[roomID].player1 = -1;
    rooms[roomID].player2 = -1;
    rooms[roomID].round = 0;
}

void drawBoard(int roomID, int turn, char *buf, int index)
{
    char tmp[Buffer_Max];
    memset(tmp, 0, sizeof(tmp));

    if (index >= 0)
    {
        rooms[roomID].board[index] = (turn) ? 'O' : 'X';
        sprintf(tmp, " %c | %c | %c \n-----------\n %c | %c | %c \n-----------\n %c | %c | %c \n", rooms[roomID].board[0], rooms[roomID].board[1], rooms[roomID].board[2], rooms[roomID].board[3], rooms[roomID].board[4], rooms[roomID].board[5], rooms[roomID].board[6], rooms[roomID].board[7], rooms[roomID].board[8]);
    }
    else
        sprintf(tmp, " 0 | 1 | 2 \n-----------\n 3 | 4 | 5 \n-----------\n 6 | 7 | 8 \n");

    strncat(buf, tmp, strlen(tmp));
}

void acceptGame(int player1_ID, int player2_ID)
{
    int readCnt, writeCnt, roomID = users[player1_ID].roomID, player1_fd = users[player1_ID].fd, player2_fd = users[player2_ID].fd;
    char buf[Buffer_Max];

    users[player1_ID].onGame = 1;
    users[player2_ID].onGame = 1;

    sprintf(buf, "Game Start, enter 0-8 to make a move, %s first\n", users[player1_ID].acc);
    drawBoard(roomID, -1, buf, -1);

    if ((writeCnt = Write(player1_fd, buf, sizeof(buf))) < 0)
    {
        sprintf(buf, "Game terminated because the other player left game");
        Write(player2_fd, buf, sizeof(buf));
        leaveGame(player2_ID);
        resetGame(roomID);
    }
    else if ((writeCnt = Write(player2_fd, buf, sizeof(buf))) < 0)
    {
        sprintf(buf, "Game terminated because the other player left game");
        Write(player1_fd, buf, sizeof(buf));
        leaveGame(player1_ID);
        resetGame(roomID);
    }
}

void rejectGame(int player1_ID, int player2_ID)
{
    int roomID = users[player1_ID].roomID, player1_fd = users[player1_ID].fd, player2_fd = users[player2_ID].fd;
    char buf[Buffer_Max];

    users[player1_ID].onGame = 1;
    users[player2_ID].onGame = 1;

    sprintf(buf, "%s refuse your invitation", users[player2_ID].acc);

    Write(player1_fd, buf, sizeof(buf));
    leaveGame(player1_ID);
    leaveGame(player2_ID);
    resetGame(roomID);
}

void sendInvitation(int selfFd, int opponentID)
{
    int writeCnt;
    char buf[Buffer_Max];

    if (opponentID >= USER_MAX)
    {
        sprintf(buf, "User not found");
        Write(selfFd, buf, sizeof(buf));
    }
    else
    {
        int roomID, selfID = userlist[selfFd], opponentFd = users[opponentID].fd;
        if (opponentFd == -1)
        {
            sprintf(buf, "%s is offline", users[opponentID].acc);
            Write(selfFd, buf, sizeof(buf));
        }
        else if (selfFd == opponentFd)
        {
            sprintf(buf, "You cannot play with yourself");
            Write(selfFd, buf, sizeof(buf));
        }
        else if(users[opponentID].onGame >= 0)  // waiting or starting state
        {
            sprintf(buf, "You cannot invite %s now", users[opponentID].acc);
            Write(selfFd, buf, sizeof(buf));
        }
        else
        {
            roomID = selectRoom();
            if (roomID < 0)
                sprintf(buf, "Sorry, there is no empty room for playing!");
            else
            {
                sprintf(buf, "%s wants to play with you, type yes or no: ", users[selfID].acc);
                if ((writeCnt = Write(opponentFd, buf, sizeof(buf))) < 0)
                {
                    sprintf(buf, "%s disconnect unexpectlly", users[opponentID].acc);
                    Write(selfFd, buf, sizeof(buf));
                    RoomActive[roomID] = 0;
                }
                else
                {
                    sprintf(buf, "Waiting...");
                    if ((writeCnt = Write(selfFd, buf, sizeof(buf))) > 0)
                    {
                        users[selfID].roomID = roomID;
                        users[selfID].onGame = 0; // waiting state
                        users[opponentID].roomID = roomID;
                        users[opponentID].onGame = 0; // waiting state
                        RoomActive[roomID] = 1;
                        rooms[roomID].player1 = selfID;
                        rooms[roomID].player2 = opponentID;
                    }
                    else
                        RoomActive[roomID] = 0;
                }
            }
        }
    }
}

void playGame(int player1_ID, int player2_ID)
{

}

void parseRequest(int connectFd)
{
    char buf[Buffer_Max];

    if (Read(connectFd, buf, sizeof(buf)) > 0)
    {
        if (strncmp(buf, "list", 4) == 0)
            listUsers(connectFd);
        else if (strncmp(buf, "logout", 6) == 0)
            userLogout(connectFd);
        else if (strncmp(buf, "PK", 2) == 0)
            sendInvitation(connectFd, atoi(buf + 3));
        else
        {
            int connectID = userlist[connectFd];

            if (strncmp(buf, "yes", 3) == 0)
            {
                if (users[connectID].onGame == 0)
                    acceptGame(rooms[users[connectID].roomID].player1, connectID);
                /* Error Handler */
            }
            else if (strncmp(buf, "no", 2) == 0)
            {
                if (users[connectID].onGame == 0)
                    rejectGame(rooms[users[connectID].roomID].player1, connectID);
                /* Error Handler */
            }
            else
            {
                if (users[connectID].onGame == 1)
                    playGame(rooms[users[connectID].roomID].player1, connectID);
            }
        }
    }
}

int main()
{
    int listenFd, connectFd;

    memset(userlist, -1, sizeof(userlist));
    signal(SIGPIPE, SIG_IGN);

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
            perror("Select error");
            continue;
        }

        for (int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (listenFd == i)
                    acceptClient(listenFd);
                else
                {
                    if (userlist[i] == -1) // Login
                        userLogin(i);
                    else
                        parseRequest(i);
                }
            }
        }
    }
    return 0;
}