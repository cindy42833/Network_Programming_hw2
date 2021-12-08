#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>

#define USER_MAX 4
#define ROOM_MAX 4
#define Buffer_MAX 1024

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
    {.board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
     .player1 = -1,
     .player2 = -1,
     .round = 0},
    {.board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
     .player1 = -1,
     .player2 = -1,
     .round = 0},
    {.board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
     .player1 = -1,
     .player2 = -1,
     .round = 0},
    {.board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
     .player1 = -1,
     .player2 = -1,
     .round = 0},
};

int RoomActive[4], userlist[64];
int fdmax, errno;
fd_set fds, read_fds;

void userLogout(int);
void helpMenu(int);

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
    char buf[Buffer_MAX];

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
                        if (write(connectFd, buf, Buffer_MAX) < 0)
                            perror("Server write error");
                    }
                    else // Login success
                    {
                        users[j].fd = connectFd; // record a user fd
                        users[j].online = 1;
                        userlist[connectFd] = j; // record user id
                        sprintf(buf, "Login Success");
                        if (write(connectFd, buf, Buffer_MAX) < 0)
                            perror("Server write error");
                        helpMenu(connectFd);
                    }
                    return;
                }
            }
            // Login fail
            sprintf(buf, "Login Fail");
            if (write(connectFd, buf, Buffer_MAX) < 0)
                perror("Server write error");
        }
    }
}

void userLogout(int connectFd)
{
    int id = userlist[connectFd];
    users[id].fd = -1;
    users[id].online = 0;

    if (users[id].onGame >= 0)
    {
        int opponentID = (id == rooms[users[id].roomID].player1) ? rooms[users[id].roomID].player2 : rooms[users[id].roomID].player1;
        char buf[Buffer_MAX];

        sprintf(buf, "Game terminated because the other player left game");
        leaveGame(opponentID);
        resetGame(users[id].roomID);
        Write(users[opponentID].fd, buf, Buffer_MAX);
    }

    users[id].onGame = -1;
    users[id].roomID = -1;
    userlist[connectFd] = -1;
    FD_CLR(connectFd, &fds);
    shutdown(connectFd, SHUT_RD);
    close(connectFd);
}

void listUsers(int connectFd)
{
    char buf[Buffer_MAX], info[128];

    sprintf(buf, "------ Online list -----\n");

    for (int i = 0; i < USER_MAX; i++)
    {
        if (users[i].online)
        {
            sprintf(info, "userId: %d, acc: %s\n", i, users[i].acc);
            strncat(buf, info, strlen(info));
        }
    }
    sprintf(info, "------------------------\n");
    strncat(buf, info, strlen(info));
    if (write(connectFd, buf, Buffer_MAX) < 0)
        perror("Server write error");
}

void helpMenu(int connectFd)
{
    char buf[Buffer_MAX], tmp[Buffer_MAX];

    sprintf(buf, "------------------------- Menu -------------------------\n");
    sprintf(tmp, "help              | list all instructions\n");
    strcat(buf, tmp);
    sprintf(tmp, "list              | list all users\n");
    strcat(buf, tmp);
    sprintf(tmp, "logout            | logout from server\n");
    strcat(buf, tmp);
    sprintf(tmp, "PK <userID>       | send invitation to other players\n");
    strcat(buf, tmp);
    sprintf(tmp, "msg <userID> <msg>| send private message to someone\n");
    strcat(buf, tmp);
    sprintf(tmp, "cancel            | cancel invitation # Use only after you send invitation or this command won't take effect\n");
    strcat(buf, tmp);
    sprintf(tmp, "--------------------------------------------------------");
    strcat(buf, tmp);

    Write(connectFd, buf, Buffer_MAX);
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

int checkMove(int roomID, char *buf, int index)
{
    if (rooms[roomID].board[index] != ' ')
    {
        sprintf(buf, "This block has filled, please select another block");
        return 0;
    }
    return 1;
}

void drawBoard(int roomID, int turn, char *buf, int index)
{
    memset(buf, 0, Buffer_MAX);

    if (index == -1) // first print
        sprintf(buf, " 0 | 1 | 2 \n-----------\n 3 | 4 | 5 \n-----------\n 6 | 7 | 8 \n");
    else
    {
        rooms[roomID].board[index] = (turn) ? 'O' : 'X';
        sprintf(buf, " %c | %c | %c \n-----------\n %c | %c | %c \n-----------\n %c | %c | %c \n", rooms[roomID].board[0], rooms[roomID].board[1], rooms[roomID].board[2], rooms[roomID].board[3], rooms[roomID].board[4], rooms[roomID].board[5], rooms[roomID].board[6], rooms[roomID].board[7], rooms[roomID].board[8]);
    }
}

void acceptGame(int player1_ID, int player2_ID)
{
    int readCnt, writeCnt, roomID = users[player1_ID].roomID, player1_fd = users[player1_ID].fd, player2_fd = users[player2_ID].fd;
    char buf[Buffer_MAX], tmp[Buffer_MAX];

    users[player1_ID].onGame = 1;
    users[player2_ID].onGame = 1;

    sprintf(buf, "Game Start, enter 0-8 to make a move, %s first\n# Type words to chat with your opponent!\n", users[player1_ID].acc);
    drawBoard(roomID, -1, tmp, -1);
    strncat(buf, tmp, strlen(tmp));

    if (Write(player1_fd, buf, Buffer_MAX) > 0)
        Write(player2_fd, buf, Buffer_MAX);
}

void rejectGame(int player1_ID, int player2_ID)
{
    int roomID = users[player1_ID].roomID, player1_fd = users[player1_ID].fd, player2_fd = users[player2_ID].fd;
    char buf[Buffer_MAX];

    users[player1_ID].onGame = 1;
    users[player2_ID].onGame = 1;

    sprintf(buf, "%s refuse your invitation", users[player2_ID].acc);

    Write(player1_fd, buf, Buffer_MAX);
    leaveGame(player1_ID);
    leaveGame(player2_ID);
    resetGame(roomID);
}

void sendInvitation(int selfFd, int opponentID)
{
    int writeCnt;
    char buf[Buffer_MAX];

    if (users[userlist[selfFd]].onGame >= 0)
    {
        sprintf(buf, "You cannot send invitation now");
        Write(selfFd, buf, Buffer_MAX);
    }
    else
    {
        if (opponentID >= USER_MAX) // User not found
        {
            sprintf(buf, "User not found");
            Write(selfFd, buf, Buffer_MAX);
        }
        else
        {
            int roomID, selfID = userlist[selfFd], opponentFd = users[opponentID].fd;
            if (opponentFd == -1) // User is offline
            {
                sprintf(buf, "%s is offline", users[opponentID].acc);
                Write(selfFd, buf, Buffer_MAX);
            }
            else if (selfFd == opponentFd) // Play with yourself
            {
                sprintf(buf, "You cannot play with yourself");
                Write(selfFd, buf, Buffer_MAX);
            }
            else if (users[opponentID].onGame >= 0) // waiting or starting state
            {
                sprintf(buf, "You cannot invite %s now", users[opponentID].acc);
                Write(selfFd, buf, Buffer_MAX);
            }
            else
            {
                roomID = selectRoom();
                if (roomID < 0)
                    sprintf(buf, "Sorry, there is no empty room for playing!");
                else
                {
                    sprintf(buf, "%s wants to play with you, type yes or no: ", users[selfID].acc);
                    if ((writeCnt = Write(opponentFd, buf, Buffer_MAX)) < 0)
                    {
                        Write(selfFd, buf, Buffer_MAX);
                        RoomActive[roomID] = 0;
                    }
                    else
                    {
                        sprintf(buf, "Waiting...");
                        if ((writeCnt = Write(selfFd, buf, Buffer_MAX)) > 0)
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
}

int checkWinner(int roomID)
{
    int ret;
    char *ptr = rooms[roomID].board;
    for (int i = 0; i < 9; i += 3)
    {
        if ((ptr[i] == ptr[i + 1] && ptr[i + 1] == ptr[i + 2]) && ptr[i] != ' ')
            return 1;
    }
    for (int i = 0; i < 3; i++)
    {
        if ((ptr[i] == ptr[i + 3] && ptr[i + 3] == ptr[i + 6]) && ptr[i] != ' ')
            return 1;
    }
    if ((ptr[0] == ptr[4] && ptr[4] == ptr[8] && ptr[0] != ' ') || (ptr[2] == ptr[4] && ptr[4] == ptr[6] && ptr[2] != ' '))
        return 1;
    return 0;
}

void playGame(int playerID, int index)
{
    int writeCnt, roomID, turn, whichPlayer, opponentID, thisPlayerFd, opponentFd;
    char buf[Buffer_MAX], tmp[Buffer_MAX];
    memset(buf, 0, Buffer_MAX);
    memset(tmp, 0, sizeof(tmp));

    roomID = users[playerID].roomID;
    turn = (rooms[roomID].round + 1) % 2;              // 1 is player1 turn, 0 is player2 turn
    whichPlayer = (rooms[roomID].player1 == playerID); // 1 is player1, 0 is player2
    opponentID = (whichPlayer) ? rooms[roomID].player2 : rooms[roomID].player1;
    thisPlayerFd = users[playerID].fd;
    opponentFd = users[opponentID].fd;

    if (turn == whichPlayer)
    {
        if (checkMove(roomID, buf, index))
        {
            rooms[roomID].round++;
            drawBoard(roomID, turn, buf, index);
            if (checkWinner(roomID) && rooms[roomID].round > 4)
            {
                sprintf(tmp, "Game end, winner is %s", users[playerID].acc);
                resetGame(roomID);
                leaveGame(playerID);
                leaveGame(opponentID);
            }
            else
            {
                if (rooms[roomID].round == 9)
                {
                    sprintf(tmp, "Game end, no winner");
                    resetGame(roomID);
                    leaveGame(playerID);
                    leaveGame(opponentID);
                }
                else
                    sprintf(tmp, "It's %s turn, enter 0-8 to make a move", users[opponentID].acc);
            }

            strncat(buf, tmp, strlen(tmp));
            if ((writeCnt = Write(thisPlayerFd, buf, Buffer_MAX)) < 0)
            {
                sprintf(buf, "Game terminated because the other player left game");
                Write(opponentFd, buf, Buffer_MAX);
                leaveGame(opponentID);
                resetGame(roomID);
            }
            else if ((writeCnt = Write(opponentFd, buf, Buffer_MAX)) < 0)
            {
                sprintf(buf, "Game terminated because the other player left game");
                Write(thisPlayerFd, buf, Buffer_MAX);
                leaveGame(playerID);
                resetGame(roomID);
            }
            return;
        }
    }
    else
        sprintf(buf, "It's not your turn");
    if (Write(thisPlayerFd, buf, Buffer_MAX) < 0)
    {
        resetGame(roomID);
        leaveGame(opponentID);
    }
}

int cancelInvitation(int connectFd)
{
    int connectID, opponentID, opponentFd, roomID;
    char buf[Buffer_MAX];

    connectID = userlist[connectFd];
    roomID = users[connectID].roomID;
    opponentID = rooms[roomID].player2;

    if (connectID == opponentID)
    {
        sprintf(buf, "You cannot cancel invitation because you are not host");
        Write(connectFd, buf, Buffer_MAX);
    }
    else
    {
        opponentFd = users[opponentID].fd;
        resetGame(roomID);
        leaveGame(connectID);
        leaveGame(opponentID);
        sprintf(buf, "Game invitation cancel");
        Write(connectFd, buf, Buffer_MAX);
        Write(opponentFd, buf, Buffer_MAX);
    }
}

void sendPrivateMsg(int fromFd, char *buf, int option)
{
    int fromID, toID, toFd;
    char msg[Buffer_MAX], tmp[Buffer_MAX];
    char *ptr = NULL;
    memset(msg, 0, Buffer_MAX);
    memset(tmp, 0, Buffer_MAX);

    if (option == 0) // send private msg
    {
        if (sscanf(buf, "%d", &toID) != 1)
        {
            sprintf(msg, "Wrong Format: msg <userID> <message>");
            Write(fromFd, msg, Buffer_MAX);
            return;
        }
        else if(toID > USER_MAX || toID < 0)
        {
            sprintf(msg, "User not foud");
            Write(fromFd, msg, Buffer_MAX);
            return;
        }
        else if(users[toID].online == 0)
        {
            sprintf(msg, "User is offline");
            Write(fromFd, msg, Buffer_MAX);
            return;
        }       
        ptr = strstr(buf, " ") + 1;
        strcpy(tmp, ptr);
    }
    else if (option == 1) // send msg to opponent
    {
        toID = rooms[users[userlist[fromFd]].roomID].player1;
        toID = (fromID == toID) ? rooms[users[userlist[fromFd]].roomID].player2 : toID;
        strncpy(tmp, buf, strlen(buf));
    }
    fromID = userlist[fromFd];
    toFd = users[toID].fd;
    sprintf(msg, "%s say to you: ", users[fromID].acc);
    strncat(msg, tmp, strlen(tmp));
    Write(toFd, msg, Buffer_MAX);
}

void parseRequest(int connectFd)
{
    int connectID;
    char buf[Buffer_MAX];

    if (Read(connectFd, buf, Buffer_MAX) > 0)
    {
        connectID = userlist[connectFd];
        if (strcmp(buf, "list") == 0)
            listUsers(connectFd);
        else if (strcmp(buf, "help") == 0)
            helpMenu(connectFd);
        else if (strcmp(buf, "logout") == 0)
            userLogout(connectFd);
        else if (strncmp(buf, "PK ", 3) == 0)
            sendInvitation(connectFd, atoi(buf + 3));
        else if (strncmp(buf, "msg ", 4) == 0)
            sendPrivateMsg(connectFd, buf+4, 0);
        else if (users[connectID].onGame == 0) // waiting state
        {
            if (strncmp(buf, "yes", 3) == 0 && users[connectID].onGame == 0)
                acceptGame(rooms[users[connectID].roomID].player1, connectID);
            else if (strncmp(buf, "no", 2) == 0 && users[connectID].onGame == 0)
                rejectGame(rooms[users[connectID].roomID].player1, connectID);
            else if (strcmp(buf, "cancel") == 0)
                cancelInvitation(connectFd);
        }
        else if (users[connectID].onGame == 1) // on game
        {
            if (isdigit(buf[0]) && strlen(buf) == 1)
                playGame(connectID, atoi(buf));
            else
                sendPrivateMsg(connectFd, buf, 1);
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