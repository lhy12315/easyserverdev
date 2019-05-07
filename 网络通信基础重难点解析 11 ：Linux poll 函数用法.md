## 网络通信基础重难点解析 11 ：Linux poll 函数用法



### Linux poll 函数用法

**poll** 函数用于检测一组文件描述符（**F**ile **D**escriptor, **fd**）上的可读可写和出错事件，其函数签名如下：

```
#include <poll.h>

int poll(struct pollfd* fds, nfds_t nfds, int timeout);
```

**参数解释：**

- **fds**：指向一个结构体数组的首个元素的指针，每个数组元素都是一个 **struct pollfd** 结构，用于指定检测某个给定的 fd 的条件；

- **nfds**：参数 **fds** 结构体数组的长度，**nfds_t** 本质上是 **unsigned long int**，其定义如下：

  ```
  typedef unsigned long int nfds_t;
  ```

- **timeout**：表示 poll 函数的超时时间，单位为毫秒。



**struct pollfd** 结构体定义如下：

```
struct pollfd {
    int   fd;         /* 待检测事件的 fd       */
    short events;     /* 关心的事件组合        */
    short revents;    /* 检测后的得到的事件类型  */
};
```

**struct pollfd**的 **events** 字段是由开发者来设置，告诉内核我们关注什么事件，而 **revents** 字段是 **poll** 函数返回时内核设置的，用以说明该 fd 发生了什么事件。**events** 和 **revents** 一般有如下取值：

|   事件宏   |                     事件描述                     | 是否可以作为输入（events） | 是否可以作为输出（revents） |
| :--------: | :----------------------------------------------: | :------------------------: | :-------------------------: |
|   POLLIN   |        数据可读（包括普通数据&优先数据）         |             是             |             是              |
|  POLLOUT   |          数据可写（普通数据&优先数据）           |             是             |             是              |
| POLLRDNORM |                  等同于 POLLIN                   |             是             |             是              |
| POLLRDBAND |     优先级带数据可读（一般用于 Linux 系统）      |             是             |             是              |
|  POLLPRI   |       高优先级数据可读，例如 TCP 带外数据        |             是             |             是              |
| POLLWRNORM |                  等同于 POLLOUT                  |             是             |             是              |
| POLLWRBAND |                 优先级带数据可写                 |             是             |             是              |
| POLLRDHUP  | TCP连接被对端关闭，或者关闭了写操作，由 GNU 引入 |             是             |             是              |
|  POPPHUP   |                       挂起                       |             否             |             是              |
|  POLLERR   |                       错误                       |             否             |             是              |
|  POLLNVAL  |                文件描述符没有打开                |             否             |             是              |



**poll** 检测一组 fd 上的可读可写和出错事件的概念与前文介绍 **select** 的事件含义一样，这里就不再赘述。**poll** 与 **select** 相比具有如下优点：

- **poll** 不要求开发者计算最大文件描述符加 1 的大小；
- 相比于 **select**，**poll** 在处理大数目的文件描述符的时候速度更快；
- **poll** 没有最大连接数的限制，原因是它是基于链表来存储的； 
- 在调用 **poll** 函数时，只需要对参数进行一次设置就好了。

我们来看一个具体的例子：

```
/**
 * 演示 poll 函数的用法，poll_server.cpp
 * zhangyl 2019.03.16
 */
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <errno.h>

//无效fd标记
#define INVALID_FD -1

int main(int argc, char* argv[])
{
    //创建一个侦听socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        std::cout << "create listen socket error." << std::endl;
        return -1;
    }
	
	//将侦听socket设置为非阻塞的
	int oldSocketFlag = fcntl(listenfd, F_GETFL, 0);
	int newSocketFlag = oldSocketFlag | O_NONBLOCK;
	if (fcntl(listenfd, F_SETFL,  newSocketFlag) == -1)
	{
		close(listenfd);
		std::cout << "set listenfd to nonblock error." << std::endl;
		return -1;
	}
	
	//复用地址和端口号
	int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on));

    //初始化服务器地址
    struct sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(3000);
    if (bind(listenfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1)
    {
        std::cout << "bind listen socket error." << std::endl;
		close(listenfd);
        return -1;
    }

	//启动侦听
    if (listen(listenfd, SOMAXCONN) == -1)
    {
        std::cout << "listen error." << std::endl;
		close(listenfd);
        return -1;
    }
	
	std::vector<pollfd> fds;
	pollfd listen_fd_info;
	listen_fd_info.fd = listenfd;
	listen_fd_info.events = POLLIN;
	listen_fd_info.revents = 0;
	fds.push_back(listen_fd_info);
	
	//是否存在无效的fd标志
	bool exist_invalid_fd;
	int n;
	while (true)
	{
		exist_invalid_fd = false;
		n = poll(&fds[0], fds.size(), 1000);
		if (n < 0)
		{
			//被信号中断
			if (errno == EINTR)
				continue;
			
			//出错，退出
			break;
		}
		else if (n == 0)
		{
			//超时，继续
			continue;
		}
		
		for (size_t i = 0; i < fds.size(); ++i)
		{
			// 事件可读
			if (fds[i].revents & POLLIN)
			{
				if (fds[i].fd == listenfd)
				{
					//侦听socket，接受新连接
					struct sockaddr_in clientaddr;
					socklen_t clientaddrlen = sizeof(clientaddr);
					//接受客户端连接, 并加入到fds集合中
					int clientfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
					if (clientfd != -1)
					{
						//将客户端socket设置为非阻塞的
						int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
						int newSocketFlag = oldSocketFlag | O_NONBLOCK;
						if (fcntl(clientfd, F_SETFL,  newSocketFlag) == -1)
						{
							close(clientfd);
							std::cout << "set clientfd to nonblock error." << std::endl;						
						} 
						else
						{
							struct pollfd client_fd_info;
							client_fd_info.fd = clientfd;
							client_fd_info.events = POLLIN;
							client_fd_info.revents = 0;
							fds.push_back(client_fd_info);
							std::cout << "new client accepted, clientfd: " << clientfd << std::endl;
						}				
					}
				}
				else 
				{
					//普通clientfd,收取数据
					char buf[64] = { 0 };
					int m = recv(fds[i].fd, buf, 64, 0);
					if (m <= 0)
					{
						if (errno != EINTR && errno != EWOULDBLOCK)
						{
							//出错或对端关闭了连接，关闭对应的clientfd，并设置无效标志位
							for (std::vector<pollfd>::iterator iter = fds.begin(); iter != fds.end(); ++ iter)
							{
								if (iter->fd == fds[i].fd)
								{
									std::cout << "client disconnected, clientfd: " << fds[i].fd << std::endl;
									close(fds[i].fd);
									iter->fd = INVALID_FD;
									exist_invalid_fd = true;
									break;
								}
							}
						}			
					}
					else
					{
						std::cout << "recv from client: " << buf << ", clientfd: " << fds[i].fd << std::endl;
					}
				}
			}
			else if (fds[i].revents & POLLERR)
			{
				//TODO: 暂且不处理
			}
			
		}// end  outer-for-loop
		
		if (exist_invalid_fd)
		{
			//统一清理无效的fd
			for (std::vector<pollfd>::iterator iter = fds.begin(); iter != fds.end(); )
			{
				if (iter->fd == INVALID_FD)
					iter = fds.erase(iter);
				else
					++iter;
			}
		}	
	}// end  while-loop
	  
	  
	//关闭所有socket
	for (std::vector<pollfd>::iterator iter = fds.begin(); iter != fds.end(); ++ iter)
		close(iter->fd);			

    return 0;
}
```

编译上述程序生成 **poll_server** 并运行，然后使用 **nc** 命令模拟三个客户端并给 **poll_server** 发送消息，效果如下：

![](http://www.hootina.org/github_easyserverdev/20190316115558.png)



> 由于 nc 命令是以 **\n** 作为结束标志的，所以 **poll_server** 收到客户端消息时显示时分两行的。



通过上面的示例代码，我们也能看出 **poll** 函数存在的一些缺点：

- 在调用 **poll** 函数时，不管有没有有意义，大量的 fd 的数组被整体在用户态和内核地址空间之间复制；
- 与 select 函数一样，poll 函数返回后，需要遍历 fd 集合来获取就绪的 fd，这样会使性能下降； 
- 同时连接的大量客户端在一时刻可能只有很少的就绪状态，因此随着监视的描述符数量的增长，其效率也会线性下降。



与 **select** 函数实现非阻塞的 connect 原理一样，我们可以使用 poll 去实现，即通过 poll 检测 clientfd 在一定时间内是否可写，示例代码如下：

```
/**
 * Linux 下使用poll实现异步的connect，linux_nonblocking_connect_poll.cpp
 * zhangyl 2019.03.16
 */
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
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
	
	pollfd event;
	event.fd = clientfd;
	event.events = POLLOUT;
	int timeout = 3000;
	if (poll(&event, 1, timeout) != 1)
	{
		close(clientfd);
		std::cout << "[poll] connect to server error." << std::endl;
		return -1;
	}
	
	if (!(event.revents & POLLOUT))
	{
		close(clientfd);
		std::cout << "[POLLOUT] connect to server error." << std::endl;
		return -1;
	}
	
	int err;
    socklen_t len = static_cast<socklen_t>(sizeof err);
    if (::getsockopt(clientfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        return -1;
        
    if (err == 0)
        std::cout << "connect to server successfully." << std::endl;
    else
    	std::cout << "connect to server error." << std::endl;
    
	//5. 关闭socket
	close(clientfd);

    return 0;
}
```

运行效果与前面的 **select** 实现这个非阻塞的 connect 一样，这里就不再给出运行效果图了。





------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)