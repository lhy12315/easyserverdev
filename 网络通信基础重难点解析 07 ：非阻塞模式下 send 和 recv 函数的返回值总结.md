## 网络通信基础重难点解析 07 ：非阻塞模式下 send 和 recv 函数的返回值总结



#### 非阻塞模式下 send 和 recv 函数的返回值总结

我们来根据前面的讨论来总结一下 **send** 和 **recv** 函数的各种返回值意义：

|   返回值 n    |                          返回值含义                          |
| :-----------: | :----------------------------------------------------------: |
|    大于 0     |                      成功发送 n 个字节                       |
|       0       |                         对端关闭连接                         |
| 小于 0（ -1） | 出错或者被信号中断或者对端 TCP 窗口太小数据发不出去（send）或者当前网卡缓冲区已无数据可收（recv） |

我们来逐一介绍下这三种情况：

- **返回值大于 0**

  对于 **send** 和 **recv** 函数返回值大于 **0**，表示发送或接收多少字节，需要注意的是，在这种情形下，我们一定要判断下 send 函数的返回值是不是我们期望发送的缓冲区长度，而不是简单判断其返回值大于 0。举个例子：

  ```
  int n = send(socket, buf, buf_length, 0)；
  if (n > 0)
  {
      printf("send data successfully\n");
  }
  ```

  很多新手会写出上述代码，虽然返回值 n 大于 0，但是实际情形下，由于对端的 TCP 窗口可能因为缺少一部分字节就满了，所以返回值 n 的值可能在 (0, buf_length] 之间，当 0 < n < buf_length 时，虽然此时 send 函数是调用成功了，但是业务上并不算正确，因为有部分数据并没发出去。你可能在一次测试中测不出 n 不等于 buf_length 的情况，但是不代表实际中不存在。所以，建议要么认为返回值 n 等于 buf_length 才认为正确，要么在一个循环中调用 send 函数，如果数据一次性发不完，记录偏移量，下一次从偏移量处接着发，直到全部发送完为止。

   ```
  //推荐的方式一
  int n = send(socket, buf, buf_length, 0)；
  if (n == buf_length)
  {
      printf("send data successfully\n");
  }
   ```



    //推荐的方式二：在一个循环里面根据偏移量发送数据
    bool SendData(const char* buf, int buf_length)
    {
        //已发送的字节数目
        int sent_bytes = 0;
        int ret = 0;
        while (true)
        {
            ret = send(m_hSocket, buf + sent_bytes, buf_length - sent_bytes, 0);
            if (ret == -1)
            {
                if (errno == EWOULDBLOCK)
                {
                    //严谨的做法，这里如果发不出去，应该缓存尚未发出去的数据，后面介绍
                    break;
                }
                else if (errno == EINTR)
                    continue;
                else
                    return false;
            }
            else if (ret == 0)
            {
                return false;
            }
    
            sent_bytes += ret;
            if (sent_bytes == buf_length)
                break;
    
            //稍稍降低 CPU 的使用率
            usleep(1);
        }
    
        return true;
    }


​    

- **返回值等于 0**

  通常情况下，如果 **send** 或者 **recv** 函数返回 **0**，我们就认为对端关闭了连接，我们这端也关闭连接即可，这是实际开发时最常见的处理逻辑。

  但是，现在还有一种情形就是，假设调用 **send** 函数传递的数据长度就是 0 呢？**send** 函数会是什么行为？对端会 **recv** 到一个 0 字节的数据吗？需要强调的是，**在实际开发中，你不应该让你的程序有任何机会去 send 0 字节的数据，这是一种不好的做法。** 这里仅仅用于实验性讨论，我们来通过一个例子，来看下 **send** 一个长度为 **0** 的数据，**send** 函数的返回值是什么？对端会 **recv** 到 **0** 字节的数据吗？

  **server** 端代码：

  ```
  /**
   * 验证recv函数接受0字节的行为，server端，server_recv_zero_bytes.cpp
   * zhangyl 2018.12.17
   */
  #include <sys/types.h> 
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <iostream>
  #include <string.h>
  #include <vector>
  
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
  		close(listenfd);
          return -1;
      }
  
  	//3.启动侦听
      if (listen(listenfd, SOMAXCONN) == -1)
      {
          std::cout << "listen error." << std::endl;
  		close(listenfd);
          return -1;
      }
  	
  	int clientfd;
   
  	struct sockaddr_in clientaddr;
  	socklen_t clientaddrlen = sizeof(clientaddr);
  	//4. 接受客户端连接
  	clientfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
  	if (clientfd != -1)
  	{         	
  		while (true)
  		{
  			char recvBuf[32] = {0};
  			//5. 从客户端接受数据,客户端没有数据来的时候会在 recv 函数处阻塞
  			int ret = recv(clientfd, recvBuf, 32, 0);
  			if (ret > 0) 
  			{
  				std::cout << "recv data from client, data: " << recvBuf << std::endl;				
  			} 
  			else if (ret == 0)
  			{
  				std::cout << "recv 0 byte data." << std::endl;
  				continue;
  			} 
  			else
  			{
  				//出错
  				std::cout << "recv data error." << std::endl;
  				break;
  			}
  		}				
  	}
  
  	
  	//关闭客户端socket
  	close(clientfd);
  	//7.关闭侦听socket
  	close(listenfd);
  
      return 0;
  }
  ```

  上述代码侦听端口号是 **3000**，代码 **55** 行调用了 **recv** 函数，如果客户端一直没有数据，程序会阻塞在这里。

**client** 端代码：

```
/**
 * 验证非阻塞模式下send函数发送0字节的行为，client端，nonblocking_client_send_zero_bytes.cpp
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
#define SEND_DATA       ""

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
		close(clientfd);
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

	//3. 不断向服务器发送数据，或者出错退出
	int count = 0;
	while (true)
	{
		//发送 0 字节的数据
		int ret = send(clientfd, SEND_DATA, 0, 0);
		if (ret == -1) 
		{
			//非阻塞模式下send函数由于TCP窗口太小发不出去数据，错误码是EWOULDBLOCK
			if (errno == EWOULDBLOCK)
			{
				std::cout << "send data error as TCP Window size is too small." << std::endl;
				continue;
			} 
			else if (errno == EINTR)
			{
				//如果被信号中断，我们继续重试
				std::cout << "sending data interrupted by signal." << std::endl;
				continue;
			} 
			else 
			{
				std::cout << "send data error." << std::endl;
				break;
			}
		}
		else if (ret == 0)
		{
			//对端关闭了连接，我们也关闭
			std::cout << "send 0 byte data." << std::endl;
		} 
		else
		{
			count ++;
			std::cout << "send data successfully, count = " << count << std::endl;
		}
		
		//每三秒发一次
		sleep(3);
	}
	
	//5. 关闭socket
	close(clientfd);

    return 0;
}
```

**client** 端连接服务器成功以后，每隔 3 秒调用 **send** 一次发送一个 0 字节的数据。除了先启动 **server** 以外，我们使用 tcpdump 抓一下经过端口 **3000** 上的数据包，使用如下命令：

```
tcpdump -i any 'tcp port 3000'
```

然后启动 **client** ，我们看下结果：

![](http://www.hootina.org/github_easyserverdev/20190313180033.png)

客户端确实是每隔 3 秒 **send** 一次数据。此时我们使用 **lsof -i -Pn** 命令查看连接状态，也是正常的：

![](http://www.hootina.org/github_easyserverdev/20190313180454.png)

然后，tcpdump 抓包结果输出中，除了连接时的三次握手数据包，再也无其他数据包，也就是说，**send** 函数发送 **0** 字节数据，**client** 的协议栈并不会把这些数据发出去。

```
[root@localhost ~]# tcpdump -i any 'tcp port 3000'
tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
listening on any, link-type LINUX_SLL (Linux cooked), capture size 262144 bytes
17:37:03.028449 IP localhost.48820 > localhost.hbci: Flags [S], seq 1632283330, win 43690, options [mss 65495,sackOK,TS val 201295556 ecr 0,nop,wscale 7], length 0
17:37:03.028479 IP localhost.hbci > localhost.48820: Flags [S.], seq 3669336158, ack 1632283331, win 43690, options [mss 65495,sackOK,TS val 201295556 ecr 201295556,nop,wscale 7], length 0
17:37:03.028488 IP localhost.48820 > localhost.hbci: Flags [.], ack 1, win 342, options [nop,nop,TS val 201295556 ecr 201295556], length 0

```

因此，**server** 端也会一直没有输出，如果你用的是 gdb 启动 **server**，此时中断下来会发现，**server** 端由于没有数据会一直阻塞在 **recv** 函数调用处（**55** 行）。

![](http://www.hootina.org/github_easyserverdev/20190313180921.png)



上述示例再次验证了，**send** 一个 0 字节的数据没有任何意思，希望读者在实际开发时，避免写出这样的代码。





------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)