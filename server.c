#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

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
    },
    {
        .board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .player1 = -1,
        .player2 = -1,
    },
    {
        .board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .player1 = -1,
        .player2 = -1,
    },
    {
        .board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .player1 = -1,
        .player2 = -1,
    },
};

int RoomActive[4];
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

void userLogout(int connectFd, int *userlist)
{
    printf("Client logout\n");
    users[*userlist].fd = -1;
    users[*userlist].online = 0;
    users[*userlist].onGame = -1;
    users[*userlist].roomID = -1;

    *userlist = -1;
    FD_CLR(connectFd, &fds);
    shutdown(connectFd, SHUT_RD);
    close(connectFd);
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
            shutdown(connectFd, SHUT_RD);
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

void leaveGame(int userID) {
    users[userID].onGame = -1;
    users[userID].roomID = -1;
}

void resetGame(int roomID)
{
    RoomActive[roomID] = 0;
    rooms[roomID].board = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    rooms[roomID].player1 = -1;
    rooms[roomID].player2 = -1;    
}

void acceptGame(int player1_ID, int player2_ID, int *userlist)
{
    int readCnt, writeCnt, roomID = users[player1_ID].roomID, player1_fd = users[player1_ID].fd, player2_fd = users[player2_ID].fd
    char buf[Buffer_Max];
    
    users[player1_ID].onGame = 1;
    users[player2_ID].onGame = 1;

    sprintf(buf, "Game Start");

    if((writeCnt = write(player1_fd, buf, sizeof(buf))) <= 0) {
        if(writeCnt == 0) {
            userLogout(player1_fd, &userlist[player1_fd]);
            sprintf(buf, "Game terminated because the other player left");

            if()
            leaveGame(player2_ID);
            resetGame(roomID);
        }
        else {  /* Error Handler */
            perror("Server write error");
        }
    }

    if((writeCnt = write(player2_fd, buf, sizeof(buf))) <=0) {
        if(writeCnt == 0) {
            userLogout(player2_fd, &userlist[player2_fd]);
            leaveGame(player1_ID);
            resetGame(roomID);
        }
        else {  /* Error Handler */
            perror("Server write error"); 
        }
    }
}

void rejectGame(int player1_ID, int player2_ID, int *userlist)
{
    int readCnt, writeCnt, roomID = users[player1_ID].roomID, player1_fd = users[player1_ID].fd, player2_fd = users[player2_ID].fd
    char buf[Buffer_Max];
    
    users[player1_ID].onGame = 1;
    users[player2_ID].onGame = 1;

    sprintf(buf, "Game Start");

    if((writeCnt = write(player1_fd, buf, sizeof(buf))) <= 0) {
        if(writeCnt == 0) {
            userLogout(player1_fd, &userlist[player1_fd]);
            leaveGame(player2_ID);
            resetGame(roomID);
        }
        else {  /* Error Handler */
            perror("Server write error");
        }
    }

    if((writeCnt = write(player2_fd, buf, sizeof(buf))) <=0) {
        if(writeCnt == 0) {
            userLogout(player2_fd, &userlist[player2_fd]);
            leaveGame(player1_ID);
            resetGame(roomID);
        }
        else {  /* Error Handler */
            perror("Server write error"); 
        }
    }
}


void sentInvitation(int *userlist, int selfFd, int opponentFd)
{
    int roomID, writeCnt, selfID = userlist[selfFd], opponentID = userlist[opponentFd];
    char buf[Buffer_Max];

    roomID = selectRoom();

    if (roomID < 0)
    {
        sprintf(buf, "Sorry, there is no empty room for playing!");
    }
    else
    {
        sprintf(buf, "%s wants to play with you, type yes or no: ", users[selfID].acc);
        if ((writeCnt = write(opponentFd, buf, sizeof(buf))) <= 0)
        {
            if (writeCnt == 0)
            {
                sprintf(buf, "%s disconnection", users[opponentID].acc);
                userLogout(opponentFd, &userlist[opponentFd]);
            }
            else
            {
                perror("Server write error");
                sprintf(buf, "Send invitation error");
            }
        }
        else
        {
            sprintf(buf, "Waiting...");
            users[selfID].roomID = roomID;
            users[selfID].onGame = 0; // waiting state
            users[opponentID].roomID = roomID;
            users[opponentID].onGame = 0; // waiting state
            RoomActive[roomID] = 1;
            rooms[roomID].player1 = selfID;
            rooms[roomID].player2 = opponentID;
            return;
        }
    }

    if ((writeCnt = write(selfFd, buf, sizeof(buf))) <= 0)
    {
        if (writeCnt == 0) {
            if(RoomActive[roomID]) {
                resetGame(roomID);
            }
            leaveGame(opponentID);
            userLogout(selfFd, &userlist[selfFd]);
        }       
        else         
            perror("Server write error");
    }
} 

void parseRequest(int connectFd, int *userlist)
{
    int readCnt, writeCnt;
    char buf[Buffer_Max];

    if ((readCnt = read(connectFd, buf, sizeof(buf))) <= 0)
    {
        if (readCnt == 0)
            userLogout(connectFd, &userlist[connectFd]);
        else
            perror("Server read error");
    }
    else
    {
        if (strncmp(buf, "list", 4) == 0)
            listUsers(connectFd);
        else if (strncmp(buf, "logout", 6) == 0)
            userLogout(connectFd, userlist);
        else if (strncmp(buf, "PK", 2) == 0)
            sentInvitation(userlist, connectFd, users[atoi(buf + 3)].fd);
        else
        {
            int connectID = userlist[connectFd];

            else if (strncmp(buf, "yes", 3) == 0) {
                if (users[connectID].onGame == 0) 
                    acceptGame(userlist[rooms[users[connectID].roomID].player1], connectID);
                /* Error Handler */
            }
            else if (strncmp(buf, "no", 2) == 0) {
                if (users[connectID].onGame == 0) 
                    rejectGame(userlist[rooms[users[connectID].roomID].player1], connectID);
                /* Error Handler */
            }
        }
    }
}

int main()
{
    int listenFd, connectFd;
    int userlist[64];
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
                        userLogin(i, &userlist[i]);
                    else
                        parseRequest(i, userlist);
                }
            }
        }
    }
    return 0;
}