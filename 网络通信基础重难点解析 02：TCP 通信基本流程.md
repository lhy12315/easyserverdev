## 网络通信基础重难点解析 02：TCP 通信基本流程



### TCP 通信基本流程

不管多么复杂的服务器或客户端程序，其网络通信的基本原理一定如下所述：

对于服务器，其通信流程一般有如下步骤：

```
1. 调用 socket 函数创建 socket（侦听socket）
2. 调用 bind 函数 将 socket绑定到某个ip和端口的二元组上
3. 调用 listen 函数 开启侦听
4. 当有客户端请求连接上来后，调用 accept 函数接受连接，产生一个新的 socket（客户端 socket）
5. 基于新产生的 socket 调用 send 或 recv 函数开始与客户端进行数据交流
6. 通信结束后，调用 close 函数关闭侦听 socket
```

对于客户端，其通信流程一般有如下步骤：

```
1. 调用 socket函数创建客户端 socket
2. 调用 connect 函数尝试连接服务器
3. 连接成功以后调用 send 或 recv 函数开始与服务器进行数据交流
4. 通信结束后，调用 close 函数关闭侦听socket
```

上述流程可以绘制成如下图示：

![20181213192725.png](http://www.hootina.org/github_easyserverdev/20181213192725.png)



对于上面的图，读者可能有疑问，为什么客户端调用 **close()** ，会和服务器端 **recv()** 函数有关。这个涉及到 **recv()** 函数的返回值意义，我们在下文中详细讲解。

服务器端实现代码：

```
/**
 * TCP服务器通信基本流程
 * zhangyl 2018.12.13
 */
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>

int main(int argc, char* argv[])
{
    //1.创建一个侦听socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        std::cout << "create listen socket error." << std::endl;
        return -1;
    }

    //2.初始化服务器地址
    struct sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(3000);
    if (bind(listenfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1)
    {
        std::cout << "bind listen socket error." << std::endl;
        return -1;
    }

	//3.启动侦听
    if (listen(listenfd, SOMAXCONN) == -1)
    {
        std::cout << "listen error." << std::endl;
        return -1;
    }

    while (true)
    {
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
		//4. 接受客户端连接
        int clientfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
        if (clientfd != -1)
        {         	
			char recvBuf[32] = {0};
			//5. 从客户端接受数据
			int ret = recv(clientfd, recvBuf, 32, 0);
			if (ret > 0) 
			{
				std::cout << "recv data from client, data: " << recvBuf << std::endl;
				//6. 将收到的数据原封不动地发给客户端
				ret = send(clientfd, recvBuf, strlen(recvBuf), 0);
				if (ret != strlen(recvBuf))
					std::cout << "send data error." << std::endl;
				
				std::cout << "send data to client successfully, data: " << recvBuf << std::endl;
			} 
			else 
			{
				std::cout << "recv data error." << std::endl;
			}
			
			close(clientfd);
        }
    }
	
	//7.关闭侦听socket
	close(listenfd);

    return 0;
}
```

客户端实现代码：

```
/**
 * TCP客户端通信基本流程
 * zhangyl 2018.12.13
 */
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>

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

    //2.连接服务器
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    serveraddr.sin_port = htons(SERVER_PORT);
    if (connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
        std::cout << "connect socket error." << std::endl;
        return -1;
    }

	//3. 向服务器发送数据
	int ret = send(clientfd, SEND_DATA, strlen(SEND_DATA), 0);
	if (ret != strlen(SEND_DATA))
	{
		std::cout << "send data error." << std::endl;
		return -1;
	}
	
	std::cout << "send data successfully, data: " << SEND_DATA << std::endl;
	
	//4. 从客户端收取数据
	char recvBuf[32] = {0};
	ret = recv(clientfd, recvBuf, 32, 0);
	if (ret > 0) 
	{
		std::cout << "recv data successfully, data: " << recvBuf << std::endl;
	} 
	else 
	{
		std::cout << "recv data error, data: " << recvBuf << std::endl;
	}
	
	//5. 关闭socket
	close(clientfd);

    return 0;
}
```



以上代码，服务器端在地址 **0.0.0.0:3000** 启动一个侦听，客户端连接服务器成功后，给服务器发送字符串"helloworld"；服务器收到后，将收到的字符串原封不动地发给客户端。

在 Linux Shell 界面输入以下命令编译服务器端和客户端：

```
# 编译 server.cpp 生成可执行文件 server   
[root@localhost testsocket]# g++ -g -o server server.cpp
# 编译 client.cpp 生成可执行文件 client
[root@localhost testsocket]# g++ -g -o client client.cpp
```

接着，我们看下执行效果，先启动服务器程序：

```
[root@localhost testsocket]# ./server
```

再启动客户端程序：

```
[root@localhost testsocket]# ./client 
```

这个时候客户端输出：

```
send data successfully, data: helloworld
recv data successfully, data: helloworld
```

服务器端输出：

```
recv data from client, data: helloworld
send data to client successfully, data: helloworld
```



以上就是 TCP socket 网络通信的基本原理，对于很多读者来说，上述代码可能很简单，更有点“玩具”的意味。但是深刻理解这两个代码片段是进一步学习开发复杂的网络通信程序的基础。而且看似很简单的代码，却隐藏了很多的玄机和原理，接下来的章节我们将以这两段代码为蓝本，逐渐深入。





------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)


