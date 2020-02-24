#include "unp.h"
void str_echo(int sockfd){
    ssize_t n;
    char buf[MAXLINE]; /*max text line length ,defined in unp.h, 4096*/
again:
    while( (n=read(sockfd,buf,MAXLINE))>0){
        Writen(sockfd,buf,n);
    }
    if(n<0 && errno == EINTR){
        goto again;
    }
    else if(n<0){
        err_sys("str_echo: read error");
    }
}

int main(int argc, char **argv){
    int listenfd,connfd;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in cliaddr,servaddr;
    listenfd=Socket(AF_INET,SOCK_STREAM,0); /*family:IPv4协议,type字节流套接字,protocol参数设为0，选择family和type组合的系统默认值*/
    
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* htonl()将主机字节序转换为网络字节序,返回值为u_long*/
    /* INADDR_ANY 转换过来就是0.0.0.0，泛指本机的意思，表示本机的所有IP */

    servaddr.sin_port = htons(SERV_PORT);
    /* htons()将主机字节序转换为网络字节序,返回值为u_short
     * unp.h中定义的SERV_PORT，值为9877*/
   
    /*将本地协议地址赋予套接字*/
    Bind(listenfd, (SA*) &servaddr,sizeof(servaddr));
    /*listen将一个未连接的套接字转换为一个被动套接字，指示内核接受指向该套接字的连接请求;第二个参数规定了内核应该为相应套接字排队的最大连接个数
     * LISTENQ在unp.h中被设置为1024*/
    Listen(listenfd,LISTENQ);

    for(;;){
        clilen=sizeof(cliaddr);
        /*
         * 从已完成连接队列队头返回下一个已完成连接
         * 如果已完成连接队列为空，那么进程被投入书面（假定套接字默认为阻塞方式）*/
        connfd=Accept(listenfd,(SA *) &cliaddr, &clilen);
        if((childpid = Fork()) ==0 ){ /*child process*/
            Close(listenfd);    /* close listening socket,子进程关闭监听套接字*/
            str_echo(connfd);  /* process the request */
            exit(0);
        }
        Close(connfd); /*parent closes connected socket,父进程关闭已连接套接字*/
    }
}
