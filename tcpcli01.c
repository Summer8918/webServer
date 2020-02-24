#include "unp.h"

void str_cli(FILE *fp,int sockfd){
    char sendline[MAXLINE], recvline[MAXLINE];

    /*Fgets()，字符串输入，若以stdin标准输入作为参数，表示从键盘输入数据
     * 将输入字符串存储在sendline数组中*/
    while(Fgets(sendline,MAXLINE,fp)!=NULL){
        Writen(sockfd,sendline,strlen(sendline));
        if(Readline(sockfd,recvline,MAXLINE) == 0){
            err_quit("str_cli:server terminated prematurely");
        }
        /*readline从服务器读入回射行，fputs把它写到标准输出*/
        Fputs(recvline,stdout);
    }
}

int main(int argc,char **argv){
    int sockfd;
    struct sockaddr_in servaddr;
    if(argc != 2){
        err_quit("usage: tcpcli <IPaddress>");
    }
    sockfd = Socket(AF_INET,SOCK_STREAM,0);
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    /* Inet_pton适用于IPv4和IPv6地址，将argv[1]指向的字符串转换为二进制值存放在servaddr.sin_addr所指地址*/
    Inet_pton(AF_INET,argv[1],&servaddr.sin_addr);
    
    /*客户用connect函数与TCP服务器建立连接*/
    Connect(sockfd,(SA*) &servaddr,sizeof(servaddr));
    str_cli(stdin,sockfd); /*do it all*/
    exit(0);
}
