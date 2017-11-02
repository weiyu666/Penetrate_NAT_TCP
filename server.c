/*
文件：server.c
PS：第一个连接上服务器的客户端，称为client1，第二个连接上服务器的客户端称为client2
这个服务器的功能是：
1：对于client1，它返回"first"，并在client2连接上之后，将client2经过转换后的IP和port发给client1；
2：对于client2，它返回client1经过转换后的IP和port和自身的port，并在随后断开与他们的连接。
*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

#define MAXLINE 128
#define SERV_PORT 8877

//发生了致命错误，退出程序
void error_quit(const char *str)    
{    
	fprintf(stderr, "%s", str);  
	//如果设置了错误号，就输入出错原因
	if( errno != 0 )
		fprintf(stderr, " : %s", strerror(errno));
	printf("\n");
	exit(1);    
}   

int main(void)      
{          
	int i, res, cur_port; 
	int connfd, firstfd, listenfd;   
	int count = 0;
	char str_ip[MAXLINE];	 //缓存IP地址
	char cur_inf[MAXLINE];	 //当前的连接信息[IP+port]
	char first_inf[MAXLINE];	//第一个链接的信息[IP+port]
	char buffer[MAXLINE];	 //临时发送缓冲区
	socklen_t clilen;      
	struct sockaddr_in cliaddr;      
	struct sockaddr_in servaddr;

	//创建用于监听TCP协议套接字        
	listenfd = socket(AF_INET, SOCK_STREAM, 0);      
	memset(&servaddr, 0, sizeof(servaddr));      
	servaddr.sin_family = AF_INET;      
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
	servaddr.sin_port = htons(SERV_PORT);      

	//把socket和socket地址结构联系起来       
	res = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));    
	if( -1 == res )
		error_quit("bind error");

	//开始监听端口       
	res = listen(listenfd, INADDR_ANY);    
	if( -1 == res )
		error_quit("listen error");

	while( 1 )
	{
		//接收来自客户端的连接
		connfd = accept(listenfd,(struct sockaddr *)&cliaddr, &clilen);  
		if( -1 == connfd )
			error_quit("accept error");
		inet_ntop(AF_INET, (void*)&cliaddr.sin_addr, str_ip, sizeof(str_ip));

		count++;
		//对于第一个链接，将其的IP+port存储到first_inf中，
		//并和它建立长链接，然后向它发送字符串'first'，
		if( count == 1 )
		{
			firstfd = connfd;
			cur_port = ntohs(cliaddr.sin_port);
			snprintf(first_inf, MAXLINE, "%s %d", str_ip, cur_port);   
			strcpy(cur_inf, "first\n");
			write(connfd, cur_inf, strlen(cur_inf)+1);
		}
		//对于第二个链接，将其的IP+port发送给第一个链接，
		//将第一个链接的信息和他自身的port返回给它自己，
		//然后断开两个链接，并重置计数器
		else if( count == 2 )
		{
			cur_port = ntohs(cliaddr.sin_port);
			snprintf(cur_inf, MAXLINE, "%s %d\n", str_ip, cur_port);
			snprintf(buffer, MAXLINE, "%s %d\n", first_inf, cur_port);
			write(connfd, buffer, strlen(buffer)+1);
			write(firstfd, cur_inf, strlen(cur_inf)+1); 
			close(connfd);
			close(firstfd);
			count = 0;
		}
		//如果程序运行到这里，那肯定是出错了
		else
			error_quit("Bad required");
	}
	return 0;
}
