/*文件：client.c
PS：第一个连接上服务器的客户端，称为client1，第二个连接上服务器的客户端称为client2
这个程序的功能是：先连接上服务器，根据服务器的返回决定它是client1还是client2，
若是client1，它就从服务器上得到client2的IP和Port，连接上client2,
若是client2，它就从服务器上得到client1的IP和Port和自身经转换后的port，
在尝试连接了一下client1后（这个操作会失败），然后根据服务器返回的port进行监听。
这样以后，就能在两个客户端之间进行点对点通信了。
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

typedef struct
{
	char ip[32];
	int port;
}server;

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

int main(int argc, char **argv)     
{          
	int i, res, port;
	int connfd, sockfd, listenfd; 
	unsigned int value = 1;
	char buffer[MAXLINE];      
	socklen_t clilen;        
	struct sockaddr_in servaddr, sockaddr, connaddr;  
	server other;

	if( argc != 2 )
		error_quit("Using: ./client <IP Address>");

	//创建用于链接（主服务器）的套接字        
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	memset(&sockaddr, 0, sizeof(sockaddr));      
	sockaddr.sin_family = AF_INET;      
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
	sockaddr.sin_port = htons(SERV_PORT);      
	inet_pton(AF_INET, argv[1], &sockaddr.sin_addr);
	//设置端口可以被重用
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

	//连接主服务器
	res = connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)); 
	if( res < 0 )
		error_quit("connect error");

	//从主服务器中读取出信息
	res = read(sockfd, buffer, MAXLINE);
	if( res < 0 )
		error_quit("read error");
	printf("Get: %s", buffer);

	//若服务器返回的是first，则证明是第一个客户端
	if( 'f' == buffer[0] )
	{
		//从服务器中读取第二个客户端的IP+port
		res = read(sockfd, buffer, MAXLINE);
		sscanf(buffer, "%s %d", other.ip, &other.port);
		printf("ff: %s %d\n", other.ip, other.port);

		//创建用于的套接字        
		connfd = socket(AF_INET, SOCK_STREAM, 0); 
		memset(&connaddr, 0, sizeof(connaddr));      
		connaddr.sin_family = AF_INET;      
		connaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
		connaddr.sin_port = htons(other.port);    
		inet_pton(AF_INET, other.ip, &connaddr.sin_addr);

		//尝试去连接第二个客户端，前几次可能会失败，因为穿透还没成功，
		//如果连接10次都失败，就证明穿透失败了（可能是硬件不支持）
		while( 1 )
		{
			static int j = 1;
			res = connect(connfd, (struct sockaddr *)&connaddr, sizeof(connaddr)); 
			if( res == -1 )
			{
				if( j >= 10 )
					error_quit("can't connect to the other client\n");
				printf("connect error, try again. %d\n", j++);
				sleep(1);
			}
			else 
				break;
		}

		strcpy(buffer, "Hello, world\n");
		//连接成功后，每隔一秒钟向对方（客户端2）发送一句hello, world
		while( 1 )
		{
			res = write(connfd, buffer, strlen(buffer)+1);
			if( res <= 0 )
				error_quit("write error");
			printf("send message: %s", buffer);
			sleep(1);
		}
	}
	//第二个客户端的行为
	else
	{
		//从主服务器返回的信息中取出客户端1的IP+port和自己公网映射后的port
		sscanf(buffer, "%s %d %d", other.ip, &other.port, &port);

		//创建用于TCP协议的套接字        
		sockfd = socket(AF_INET, SOCK_STREAM, 0); 
		memset(&connaddr, 0, sizeof(connaddr));      
		connaddr.sin_family = AF_INET;      
		connaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
		connaddr.sin_port = htons(other.port);      
		inet_pton(AF_INET, other.ip, &connaddr.sin_addr);
		//设置端口重用
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

		//尝试连接客户端1，肯定会失败，但它会在路由器上留下记录，
		//以帮忙客户端1成功穿透，连接上自己 
		res = connect(sockfd, (struct sockaddr *)&connaddr, sizeof(connaddr)); 
		if( res < 0 )
			printf("connect error\n");

		//创建用于监听的套接字        
		listenfd = socket(AF_INET, SOCK_STREAM, 0); 
		memset(&servaddr, 0, sizeof(servaddr));      
		servaddr.sin_family = AF_INET;      
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
		servaddr.sin_port = htons(port);
		//设置端口重用
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

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
			//接收来自客户端1的连接
			connfd = accept(listenfd,(struct sockaddr *)&sockaddr, &clilen);  
			if( -1 == connfd )
				error_quit("accept error");

			while( 1 )
			{
				//循环读取来自于客户端1的信息
				res = read(connfd, buffer, MAXLINE);
				if( res <= 0 )
					error_quit("read error");
				printf("recv message: %s", buffer);
			}
			close(connfd);
		}
	}

	return 0;
}
