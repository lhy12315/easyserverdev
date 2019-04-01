/**
 * 异步的connect写法，nonblocking_connect.cpp
 * zhangyl 2018.12.17
 */
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT     3000
#define SEND_DATA       "helloworld"

int main(int argc, char* argv[])
{
    //1.创建一个socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cout << "create client socket error." << std::endl;
        return -1;
    }
	
	//连接成功以后，我们再将 clientfd 设置成非阻塞模式，
	//不能在创建时就设置，这样会影响到 connect 函数的行为
	int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
	int newSocketFlag = oldSocketFlag | O_NONBLOCK;
	if (fcntl(clientfd, F_SETFL,  newSocketFlag) == -1)
	{
		close(clientfd);
		std::cout << "set socket to nonblock error." << std::endl;
		return -1;
	}

    //2.连接服务器
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    serveraddr.sin_port = htons(SERVER_PORT);
	for (;;)
	{
		int ret = connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
		if (ret == 0)
		{
			std::cout << "connect to server successfully." << std::endl;
			close(clientfd);
			return 0;
		} 
		else if (ret == -1) 
		{
			if (errno == EINTR)
			{
				//connect 动作被信号中断，重试connect
				std::cout << "connecting interruptted by signal, try again." << std::endl;
				continue;
			} else if (errno == EINPROGRESS)
			{
				//连接正在尝试中
				break;
			} else {
				//真的出错了，
				close(clientfd);
				return -1;
			}
		}
	}
	
	fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(clientfd, &writeset);
	//可以利用tv_sec和tv_usec做更小精度的超时控制
    struct timeval tv;
    tv.tv_sec = 3;  
    tv.tv_usec = 0;
    if (select(clientfd + 1, NULL, &writeset, NULL, &tv) == 1)
    {
		std::cout << "[select] connect to server successfully." << std::endl;
	} else {
		std::cout << "[select] connect to server error." << std::endl;
	}

	//5. 关闭socket
	close(clientfd);

    return 0;
}