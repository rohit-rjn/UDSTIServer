/**Ticket Issue server,
In this case all the mallServers are assumed to be on the "same remote machine"
and the clients contact the server for address of the mallServer.
**/
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<sys/time.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<pthread.h>
#include <sys/poll.h>
#include<semaphore.h>
#include<stdlib.h>

#define MYPORT 3490
#define BACKLOG 20

pthread_t mallThread[1000], clientThread[1000]; //threads for handling malls and clients
int numClients=0, numMalls=0;
int stopIssue = 0;

struct client
{
    int fd;
    int ver;    //used for verification
    int tryCount;
};

struct mall
{
    int fd;
    char IP[100];
    int port;
};

struct ack
{
    char IP[100];
    int port;
};

struct client c[1000];
struct mall m[1000];

void clientFunc(void *n)
{
    struct ack a;
    int i = *(int*)n, ma;
    int fd = c[i].fd, id = htons(i), v = htons(c[i].ver);
    send(fd, &id, sizeof(id), 0);   //send the client number
    sleep(1);
    send(fd, &v, sizeof(v), 0);   //send the verification number
    recv(fd, &ma, sizeof(m), 0);
    ma = ntohs(ma); //requested mall no.
    strcpy(a.IP, m[ma].IP);
    a.port =m[ma].port; //already in network-order
    send(fd, &a, sizeof(struct ack), 0);    //send address of the mall requested
    close(fd);

}

void mallFunc(void *n)
{
    struct ack a;
    int i = *(int*)n, len, p;
    char buf[1000];
    printf("Servicing Mall: %d\n", i);
    int fd = m[i].fd, id = htons(i);
    send(fd, &id, sizeof(id), 0);   //send the mall number
    len = recv(fd, buf, 17, 0);   //recv IP address
    buf[len] = '\0';
    strcpy(m[i].IP, buf);
    recv(fd, &p, sizeof(int), 0);   //recv the port number
    m[i].port = p;
    printf("Received len: %d IP: %s and port: %d\n", len, m[i].IP, ntohs(p));
    usleep(10000);
    int req, res;
    while(1)
    {
        int idf, v;
        //when a verification starts, the client id remains the same till the next verification request comes from this mallClient
        recv(fd, &req, sizeof(req), 0);     //accept request code
        if(ntohs(req)==0)   //code for verification
        {
            recv(fd, &idf, sizeof(id), 0);   //id of the client
            recv(fd, &v, sizeof(v), 0);     //verification of the client
            idf = ntohs(idf);
            v = ntohs(v);
            if(c[idf].ver == v)      //if verified
            {
                res = htons(1);
                send(fd, &res, sizeof(res), 0);
            }
            else
            {
                res = htons(0);
                send(fd, &res, sizeof(res), 0);
            }
        }
        else if(ntohs(req)==1)  //for next mall's address
        {
            //increase the client's trycount
            c[idf].tryCount += 1;
            if(c[idf].tryCount == numMalls)     //if already checked all other malls
            {
                c[idf].tryCount == 0;   //reset back to zero
                res = htons(0);
                send(fd, &res, sizeof(res), 0);
            }
            else
            {
                res = htons(1);
                send(fd, &res, sizeof(res), 0);
                usleep(10000);
                strcpy(a.IP, m[(i%numMalls)+1].IP);    //where 'i' is the mall number being serviced by this thread
                a.port = m[(i%numMalls)+1].port;
                send(fd, &a, sizeof(a), 0);
            }
        }
    }
}

int main()
{
    char h[1000];
    size_t size;
//    gethostname(h, size);
//    h[size] = '\0';
//    printf("-->%s\n", h);
    fd_set readfd;
    int gate1, gate2, gate3, mallSfd, fdmax;
    int yes = 1;
    int i, j, k, l, n;//looping vars

    struct sockaddr_in my_addr1, my_addr2, my_addr3, my_addr4, their_addr, their_addr1;

    my_addr1.sin_family = AF_INET;
    my_addr1.sin_port = htons(MYPORT);
    my_addr1.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr1.sin_zero), '\0', 8);
    my_addr2.sin_family = AF_INET;
    my_addr2.sin_port = htons(MYPORT+1);
    my_addr2.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr2.sin_zero), '\0', 8);
    my_addr3.sin_family = AF_INET;
    my_addr3.sin_port = htons(MYPORT+2);
    my_addr3.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr3.sin_zero), '\0', 8);
    my_addr4.sin_family = AF_INET;
    my_addr4.sin_port = htons(MYPORT+3);
    my_addr4.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr4.sin_zero), '\0', 8);

    gate1 = socket(PF_INET, SOCK_STREAM, 0);
    gate2 = socket(PF_INET, SOCK_STREAM, 0);
    gate3 = socket(PF_INET, SOCK_STREAM, 0);
    mallSfd = socket(PF_INET, SOCK_STREAM, 0);

    if(bind(gate1, (struct sockaddr *)&my_addr1, sizeof(struct sockaddr)) == -1)
    {
        perror("Bind error\n");
    }
    if (setsockopt(gate1,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    fdmax = gate1;
    yes=1;
    if(bind(gate2, (struct sockaddr *)&my_addr2, sizeof(struct sockaddr)) == -1)
    {
        perror("Bind error\n");
    }
    if (setsockopt(gate2,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    fdmax = (gate2 > fdmax ? gate2 : fdmax);
    yes=1;
    if(bind(gate3, (struct sockaddr *)&my_addr3, sizeof(struct sockaddr)) == -1)
    {
        perror("Bind error\n");
    }
    if (setsockopt(gate3,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    fdmax = (gate3 > fdmax ? gate3 : fdmax);
    yes=1;
    if(bind(mallSfd, (struct sockaddr *)&my_addr4, sizeof(struct sockaddr)) == -1)
    {
        perror("Bind error\n");
    }
    if (setsockopt(mallSfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    fdmax = (mallSfd > fdmax ? mallSfd : fdmax);

    listen(gate1, 50);
    listen(gate2, 50);
    listen(gate3, 50);
    listen(mallSfd, 50);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int newfd, id[1000],id1[1000];

    socklen_t sin_size = sizeof(struct sockaddr);
    char greetings[1000];
    strcpy(greetings, "You have reached the Ticket-Issue server");

    while(1)
    {
        FD_SET(gate1, &readfd);
        FD_SET(gate2, &readfd);
        FD_SET(gate3, &readfd);
        FD_SET(mallSfd, &readfd);

        select(fdmax+1, &readfd, NULL, NULL, &tv);
        if(FD_ISSET(gate1, &readfd))
        {
            newfd = accept(gate1, (struct sockaddr *)&their_addr, &sin_size);
            if(newfd != -1)
            {
                send(newfd, greetings, strlen(greetings), 0);
                id[++numClients] = numClients;
                c[numClients].fd = newfd;
                c[numClients].tryCount = 0;
                c[numClients].ver = rand()%1000;
                //assign thread
                pthread_create(&clientThread[numClients%999], NULL, (void*)&clientFunc, (void*)&id[numClients]);
            }
        }
        if(FD_ISSET(gate2, &readfd))
        {
            newfd = accept(gate2, (struct sockaddr *)&their_addr, &sin_size);
            if(newfd != -1)
            {
                send(newfd, greetings, strlen(greetings), 0);
                id[++numClients] = numClients;
                c[numClients].fd = newfd;
                c[numClients].tryCount = 0;
                c[numClients].ver = rand()%1000;
                //assign thread
                pthread_create(&clientThread[numClients%999], NULL, (void*)&clientFunc, (void*)&id[numClients]);
            }
        }
        if(FD_ISSET(gate3, &readfd))
        {
            newfd = accept(gate3, (struct sockaddr *)&their_addr, &sin_size);
            if(newfd != -1)
            {
                send(newfd, greetings, strlen(greetings), 0);
                id[++numClients] = numClients;
                c[numClients].fd = newfd;
                c[numClients].tryCount = 0;
                c[numClients].ver = rand()%1000;
                //assign thread
                pthread_create(&clientThread[numClients%999], NULL, (void*)&clientFunc, (void*)&id[numClients]);
            }
        }
        if(FD_ISSET(mallSfd, &readfd))
        {
            newfd = accept(mallSfd, (struct sockaddr *)&their_addr, &sin_size);
            if(newfd != -1)
            {
                send(newfd, greetings, strlen(greetings), 0);
                id1[++numMalls] = numMalls;
                m[numMalls].fd = newfd;
                //assign thread
                pthread_create(&mallThread[numMalls], NULL, (void*)&mallFunc, (void*)&id1[numMalls]);
            }
        }
    }

    return 0;
}
