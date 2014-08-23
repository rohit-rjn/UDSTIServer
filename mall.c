/**
This is the mall-server.
All malls are on a single system, and hence, communicate via IPC
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

//#define MYPORT 3990
#define BACKLOG 20
#define CONTROLLEN  CMSG_LEN(sizeof(int))
int myID, sfd, ssfd, numClients=0, capacity = 2, cur = 0;
char myIP[100];
pthread_t clientThread[1000];
sem_t lock;

struct ack
{
    char IP[100];
    int port;
};

int verifyCredentials(int server, int id, int ver)
{
    int req, res, count;
    printf("Sending (%d, %d) for verification to TIServer\n", ntohs(id), ntohs(ver));
    //send for confirmation to Server
    req = htons(0); //0 for verification
    send(ssfd, &req, sizeof(req), 0);
    usleep(10000);
    send(ssfd, &id, sizeof(id), 0);
    usleep(10000);
    send(ssfd, &ver, sizeof(ver), 0);
    //receive response, 0 for failure, 1 for success
    recv(ssfd, &res, sizeof(res), 0);
    return res;
}

int recv_fd(int sfd)
{
    int fds;
    struct msghdr msg;
    struct cmsghdr *cmptr;
    cmptr = (struct cmsghdr *) malloc(CONTROLLEN);
    struct iovec iov[1];
    char b[2];
    b[0] = 0;
    b[1] = 0;
    iov[0].iov_base = b;
    iov[0].iov_len = 2;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name    = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = cmptr;
    msg.msg_controllen = CONTROLLEN;
    //sleep(3);
    fflush(stdout);
    int l = recvmsg(sfd, &msg, 0);
    //printf("length recvd %d\n", msg.msg_controllen);
    if(l>0)
    {
        cmptr = CMSG_FIRSTHDR(&msg);
        fds = *(int *)CMSG_DATA(cmptr);
        printf("received nsfd from server1: %d\n", fds);
    }
    return fds;
}

void send_fd(int sfd, int fd)
{
    struct msghdr msg;
    struct iovec iov[1];
    char b[2];
    b[0] = 0;
    b[1] = 0;
    iov[0].iov_base = b;
    iov[0].iov_len = 2;
    struct cmsghdr *cmptr = (struct cmsghdr *) malloc(CONTROLLEN);
    cmptr->cmsg_level  = SOL_SOCKET;
    cmptr->cmsg_type   = SCM_RIGHTS;
    cmptr->cmsg_len    = CONTROLLEN;
    msg.msg_controllen =  CONTROLLEN;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    *(int *)CMSG_DATA(cmptr) = fd;
    msg.msg_control    = cmptr;
    int k = sendmsg(sfd, &msg, 0);
    if(k > 0)
    {
        printf("fd passed to next interviewServer\n");
    }
}

void clientFunc(void *f)
{
    int req, res;
    int id, ver;
    int fd =*(int*)f;
    recv(fd, &id, sizeof(id), 0);
    recv(fd, &ver, sizeof(ver), 0);
    printf("Sending (%d, %d) for verification to TIServer\n", ntohs(id), ntohs(ver));

    //send for confirmation to Server
    req = htons(0); //0 for verification
    send(ssfd, &req, sizeof(req), 0);
    usleep(100000);
    send(ssfd, &id, sizeof(id), 0);
    usleep(100000);
    send(ssfd, &ver, sizeof(ver), 0);
    //receive response, 0 for failure, 1 for success
    recv(ssfd, &res, sizeof(res), 0);
    if(ntohs(res) == 1)//successful verification
    {
        printf("verified\n");
        if(cur < capacity)
        {
            //allow client to be serviced
            send(fd, &res, sizeof(res), 0);
            sem_wait(&lock);
            cur += 1;
            sem_post(&lock);
            sleep(500);
            close(fd);
            sem_wait(&lock);
            cur -= 1;
            sem_post(&lock);
        }
        else
        {
            //capacity full, take the address of the next
            req = htons(1);     //1 for next mall's address
            send(ssfd, &req, sizeof(req), 0);
            recv(ssfd, &res, sizeof(res), 0);   //0 for failure, 1 for success
            if(ntohs(res)==0)
            {
                send(fd, &res, sizeof(res), 0);
                close(fd);     //exit connection as the capacity is full in all the malls
            }
            else if(ntohs(res)==1)
            {
                struct ack a;
                char buf[1000];
                int len, port;
                res = htons(2);     //2 to continue connecting other malls
                send(fd, &res, sizeof(res), 0);
//                len = recv(ssfd, buf, 17, 0);
//                buf[len] = '\0';
//                strcpy(a.IP, buf);
                recv(ssfd, &a, sizeof(a), 0);
//                a.port = port;
                send(fd, &a, sizeof(a), 0);
            }
        }
    }
    else
    {
        printf("not verified\n");
        send(fd, &res, sizeof(res), 0);    //unsuccessful verification
        sleep(5);
        close(fd);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if(argc < 4)
    {
        printf("usage: %s ServerIP mallIP mallPort pathname\n", argv[0]);
        exit(1);
    }
    sem_init(&lock, 0, 1);
    strcpy(myIP, argv[2]);
    int yes=1;
    struct sockaddr_in my_addr, their_addr, their_addr1;
    int MYPORT = htons(atoi(argv[3]));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = MYPORT;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr.sin_zero), '\0', 8);

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(3493);
    inet_aton(argv[1], &their_addr.sin_addr);
    memset(&(their_addr.sin_zero), '\0', 8);

    sfd = socket(PF_INET, SOCK_STREAM, 0);
    if(bind(sfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Bind error\n");
    }
    if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    listen(sfd, 50);

    ssfd = socket(PF_INET, SOCK_STREAM, 0); //for the ticket-issue server

    int len, newfd, fds[1000];
    char buf[1000];
    while(connect(ssfd, (struct sockaddr *)&their_addr, sizeof(their_addr)) == 0) //connect to server
    {
        printf("-->%s:%d", myIP, ntohs(MYPORT));
        len = recv(ssfd, buf, 1000, 0); //recv greetings from the server
        buf[len] = '\0';
        printf("%s\n", buf);
        recv(ssfd, &myID, sizeof(myID), 0);
        //myID = ntohs(myID);
        printf("ID issued by the server: %d\n", ntohs(myID));

        //send ip and port of mall-client
        send(ssfd, myIP, strlen(myIP), 0);
        sleep(1);
        send(ssfd, &MYPORT, sizeof(MYPORT), 0);
        socklen_t sin_size = sizeof(struct sockaddr_in);
        while((newfd = accept(sfd, (struct sockaddr *)&their_addr1, &sin_size)) != -1)
        {
            fds[++numClients] = newfd;
            send(newfd, "Greetings, You have reached Mall ", strlen("Greetings, You have reached Mall "), 0);
            sleep(1);
            send(newfd, &myID, sizeof(myID), 0);
            pthread_create(&clientThread[numClients], NULL, (void*)&clientFunc, (void*)&fds[numClients]);
        }
    }
}
