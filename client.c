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

struct ack
{
    char IP[100];
    int port;
};

int main(int argc, char *argv[])
{
    struct sockaddr_in their_addr, mall_addr;

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &their_addr.sin_addr);
    memset(&(their_addr.sin_zero), '\0', 8);

    mall_addr.sin_family = AF_INET;
    //mall_addr.sin_port = htons(atoi(argv[2]));
    //inet_aton(argv[1], &mall_addr.sin_addr);
    memset(&(mall_addr.sin_zero), '\0', 8);

    int sfd, mallSfd, yes = 1;

    sfd = socket(PF_INET, SOCK_STREAM, 0);
    mallSfd = socket(PF_INET, SOCK_STREAM, 0);
//    if(bind(sfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
//    {
//        perror("Bind error\n");
//    }
//    if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
//    {
//        perror("setsockopt");
//        exit(1);
//    }

    int len, myID, myVer, mall;//id and verification number
    char buf[1000];
    struct ack a;

    //connect to the ticket-issue server
    while(connect(sfd, (struct sockaddr *)&their_addr, sizeof(their_addr)) == 0)
    {
        //connected
        len = recv(sfd, buf, 1000, 0);
        buf[len] = '\0';
        printf("%s\n", buf);
        usleep(100000);
        recv(sfd, &myID, sizeof(myID), 0);
        printf("ID supplied by the server: %d\n", ntohs(myID));
        recv(sfd, &myVer, sizeof(myVer), 0);
        printf("Verification supplied by the server: %d\n", ntohs(myVer));
        printf("Enter the mall no: ");
        scanf("%d", &mall);
        mall = htons(mall);
        send(sfd, &mall, sizeof(mall), 0);

        //recv address
        recv(sfd, &a, sizeof(a), 0);
        close(sfd);
        mall_addr.sin_port = a.port;
        inet_aton(a.IP, &mall_addr.sin_addr);

        int myCount = 0;    //count of all the malls contacted

        if(connect(mallSfd, (struct sockaddr*)&mall_addr, sizeof(mall_addr)) == 0)
        {
            //connected to the primary mall
            while(1)
            {
                //connected to the mall
                len = recv(mallSfd, buf, 1000, 0);
                buf[len] = '\0';
                printf("%s", buf);
                recv(mallSfd, &mall, sizeof(mall), 0);
                printf(": %d\n", ntohs(mall));
                int id, ver;
                printf("Please Enter your id and verification:\n");
                scanf("%d", &id);
                scanf("%d", &ver);
                id = htons(id);
                ver = htons(ver);
                send(mallSfd, &id, sizeof(id), 0);
                sleep(1);
                send(mallSfd, &ver, sizeof(ver), 0);    //sent both id and verification to the mall
                //wait for confirmation
                int res;
                recv(mallSfd, &res, sizeof(res), 0);
                if(ntohs(res)==1)      //successful, will be serviced by this mall
                {
                    //send the myCount to the mall
                    int c = htons(myCount);
                    send(mallSfd, &c, sizeof(c), 0);
                    //wait for confirmation
                    recv(mallSfd, &res, sizeof(res), 0);
                    if(ntohs(res)==0)
                    {
                        //has already tried all the malls
                        printf("Try after some time, no Mall has an empty slot.. exiting for now\n");
                        exit(0);
                    }
                    printf("Entry Authenticated to Mall: %d\n", ntohs(mall));
                    sleep(500);
                    exit(1);
                }
                else if(ntohs(res)==0)     //unsuccessful, due to faulty verification
                {
                    printf("Try after some time due to faulty verification..exiting for now\n");
                    exit(1);
                }
                else if(ntohs(res)==2)
                {
                    printf("Exiting from Mall: %d\n", ntohs(mall));
                    //recv next address
                    recv(mallSfd, &a, sizeof(a), 0);
                    close(mallSfd);
                    mall_addr.sin_port = a.port;
                    inet_aton(a.IP, &mall_addr.sin_addr);
                }
            }
            mallSfd = socket(PF_INET, SOCK_STREAM, 0);
        }
    }

    return 0;
}

