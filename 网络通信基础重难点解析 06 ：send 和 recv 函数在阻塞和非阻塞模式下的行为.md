## 网络通信基础重难点解析 06 ：send 和 recv 函数在阻塞和非阻塞模式下的行为



#### send 和 recv 函数在阻塞和非阻塞模式下的行为

send 和 recv 函数其实名不符实。

send 函数本质上并不是往网络上发送数据，而是将应用层发送缓冲区的数据拷贝到内核缓冲区（下文为了叙述方便，我们以“网卡缓冲区”代指）中去，至于什么时候数据会从网卡缓冲区中真正地发到网络中去要根据 TCP/IP 协议栈的行为来确定，这种行为涉及到一个叫 nagel 算法和 TCP_NODELAY 的 socket 选项，我们将在《**nagle算法与 TCP_NODELAY**》章节详细介绍。

recv 函数本质上也并不是从网络上收取数据，而只是将内核缓冲区中的数据拷贝到应用程序的缓冲区中，当然拷贝完成以后会将内核缓冲区中该部分数据移除。

可以用下面一张图来描述上述事实：

![](http://www.hootina.org/github_easyserverdev/20181217213229.png)

通过上图我们知道，不同的程序进行网络通信时，发送的一方会将内核缓冲区的数据通过网络传输给接收方的内核缓冲区。在应用程序 A 与 应用程序 B 建立了 TCP 连接之后，假设应用程序 A 不断调用 send 函数，这样数据会不断拷贝至对应的内核缓冲区中，如果 B 那一端一直不调用  recv 函数，那么 B 的内核缓冲区被填满以后，A 的内核缓冲区也会被填满，此时 A 继续调用 send 函数会是什么结果呢？ 具体的结果取决于该 socket 是否是阻塞模式。我们这里先给出结论：

- 当 socket 是阻塞模式的，继续调用 send/recv 函数会导致程序阻塞在 send/recv 调用处。
- 当 socket 是非阻塞模式，继续调用 send/recv 函数，send/recv 函数不会阻塞程序执行流，而是会立即出错返回，我们会得到一个相关的错误码，Linux 平台上该错误码为 EWOULDBLOCK 或 EAGAIN（这两个错误码值相同），Windows 平台上错误码为 WSAEWOULDBLOCK。

我们实际来编写一下代码来验证一下以上说的两种情况。

##### socket 阻塞模式下的 send 行为

服务端代码（blocking_server.cpp）如下：

```
/**
 * 验证阻塞模式下send函数的行为，server端
 * zhangyl 2018.12.17
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

    while (true)
    {
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
		//4. 接受客户端连接
        int clientfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
        if (clientfd != -1)
        {         	
			//只接受连接，不调用recv收取任何数据
			std:: cout << "accept a client connection." << std::endl;
        }
    }
	
	//7.关闭侦听socket
	close(listenfd);

    return 0;
}
```



客户端代码（blocking_client.cpp）如下：

```
/**
 * 验证阻塞模式下send函数的行为，client端
 * zhangyl 2018.12.17
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
        close(clientfd);
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

	//3. 不断向服务器发送数据，或者出错退出
	int count = 0;
	while (true)
	{
		int ret = send(clientfd, SEND_DATA, strlen(SEND_DATA), 0);
		if (ret != strlen(SEND_DATA))
		{
			std::cout << "send data error." << std::endl;
			break;
		} 
		else
		{
			count ++;
			std::cout << "send data successfully, count = " << count << std::endl;
		}
	}
	
	//5. 关闭socket
	close(clientfd);

    return 0;
}
```

在 shell 中分别编译这两个 cpp 文件得到两个可执行程序 **blocking_server** 和 **blocking_client**：

```
g++ -g -o blocking_server blocking_server.cpp
g++ -g -o blocking_client blocking_client.cpp
```

我们先启动 **blocking_server**，然后用 gdb 启动 **blocking_client**，输入 **run** 命令让 **blocking_client**跑起来，**blocking_client** 会不断地向 **blocking_server** 发送"**helloworld**"字符串，每次 send 成功后，会将计数器 **count** 的值打印出来，计数器会不断增加，程序运行一段时间后，计数器 **count** 值不再增加且程序不再有输出。操作过程及输出结果如下：

**blocking_server** 端：

```
[root@localhost testsocket]# ./blocking_server 
accept a client connection.
```

```
[root@localhost testsocket]# gdb blocking_client
GNU gdb (GDB) Red Hat Enterprise Linux 7.6.1-100.el7_4.1
Copyright (C) 2013 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "x86_64-redhat-linux-gnu".
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>...
Reading symbols from /root/testsocket/blocking_client...done.
(gdb) run
//输出结果太多，省略部分...
send data successfully, count = 355384
send data successfully, count = 355385
send data successfully, count = 355386
send data successfully, count = 355387
send data successfully, count = 355388
send data successfully, count = 355389
send data successfully, count = 355390
```

此时程序不再有输出，说明我们的程序应该“卡在”某个地方，继续按 **Ctrl + C** 让 gdb 中断下来，输入 **bt** 命令查看此时的调用堆栈，我们发现我们的程序确实阻塞在 **send** 函数调用处：

```
^C
Program received signal SIGINT, Interrupt.
0x00007ffff72f130d in send () from /lib64/libc.so.6
(gdb) bt
#0  0x00007ffff72f130d in send () from /lib64/libc.so.6
#1  0x0000000000400b46 in main (argc=1, argv=0x7fffffffe598) at blocking_client.cpp:41
(gdb) 
```

上面的示例验证了如果一端一直发数据，而对端应用层一直不取数据（或收取数据的速度慢于发送速度），则很快两端的内核缓冲区很快就会被填满，导致发送端调用 send 函数被阻塞。这里说的“**内核缓冲区**” 其实有个专门的名字，即 TCP 窗口。也就是说 socket 阻塞模式下， send 函数在 TCP 窗口太小时的行为是阻塞当前程序执行流（即阻塞 send 函数所在的线程的执行）。

说点题外话，上面的例子，我们每次发送一个“**helloworld**”（10个字节），一共发了 355390 次（每次测试的结果略有不同），我们可以粗略地算出 TCP 窗口的大小大约等于 1.7 M左右 （10 * 355390 / 2）。

让我们再深入一点，我们利用 Linux tcpdump 工具来动态看一下这种情形下 TCP 窗口大小的动态变化。需要注意的是，Linux 下使用 tcpdump 这个命令需要有 root 权限。

我们开启三个 shell 窗口，在第一个窗口先启动 **blocking_server** 进程，在第二个窗口用 tcpdump 抓经过 TCP 端口 3000 上的数据包：

```
[root@localhost testsocket]# tcpdump -i any -nn -S 'tcp port 3000'    
tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
listening on any, link-type LINUX_SLL (Linux cooked), capture size 262144 bytes
```

接着在第三个 shell 窗口，启动 **blocking_client**。当 **blocking_client** 进程不再输出时，我们抓包的结果如下：

```
[root@localhost testsocket]# tcpdump -i any -nn -S 'tcp port 3000' 
tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
listening on any, link-type LINUX_SLL (Linux cooked), capture size 262144 bytes
11:52:35.907381 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [S], seq 1394135076, win 43690, options [mss 65495,sackOK,TS val 78907688 ecr 0,nop,wscale 7], length 0
20:32:21.261484 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [S.], seq 1233000591, ack 1394135077, win 43690, options [mss 65495,sackOK,TS val 78907688 ecr 78907688,nop,wscale 7], length 0
11:52:35.907441 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907615 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135077:1394135087, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907626 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135087, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907785 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135087:1394135097, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907793 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135097, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907809 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135097:1394135107, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907814 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135107, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907839 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135117:1394135127, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907853 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135127:1394135137, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907880 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135147:1394135157, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907896 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135167, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907920 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135177:1394135187, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907924 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135187, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907938 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135197, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.923799 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], seq 1394135247:1394157135, ack 1233000592, win 342, options [nop,nop,TS val 78907704 ecr 78907688], length 21888
11:52:35.923840 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394157135, win 1365, options [nop,nop,TS val 78907704 ecr 78907688], length 0
11:52:35.923851 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394157135:1394157137, ack 1233000592, win 342, options [nop,nop,TS val 78907704 ecr 78907704], length 2
11:52:35.964013 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394157137, win 1365, options [nop,nop,TS val 78907744 ecr 78907704], length 0
11:52:35.964036 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394157137:1394170597, ack 1233000592, win 342, options [nop,nop,TS val 78907744 ecr 78907744], length 13460
11:52:35.964043 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394170597, win 2388, options [nop,nop,TS val 78907744 ecr 78907744], length 0
11:52:36.081009 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394170597:1394170607, ack 1233000592, win 342, options [nop,nop,TS val 78907861 ecr 78907744], length 10
11:52:36.121161 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394170607, win 2388, options [nop,nop,TS val 78907901 ecr 78907861], length 0
11:52:36.121252 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394170607:1394188957, ack 1233000592, win 342, options [nop,nop,TS val 78907902 ecr 78907901], length 18350
11:52:36.121285 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394188957, win 3411, options [nop,nop,TS val 78907902 ecr 78907902], length 0
11:52:36.243907 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394188957:1394188967, ack 1233000592, win 342, options [nop,nop,TS val 78908024 ecr 78907902], length 10
11:52:36.284003 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394188967, win 3411, options [nop,nop,TS val 78908064 ecr 78908024], length 0
11:52:36.284024 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394188967:1394207257, ack 1233000592, win 342, options [nop,nop,TS val 78908064 ecr 78908064], length 18290
11:52:36.284034 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394207257, win 3635, options [nop,nop,TS val 78908064 ecr 78908064], length 0
11:52:36.409664 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394207257:1394207267, ack 1233000592, win 342, options [nop,nop,TS val 78908190 ecr 78908064], length 10
11:52:36.450133 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394207267, win 3635, options [nop,nop,TS val 78908230 ecr 78908190], length 0
11:52:36.450187 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394207267:1394224477, ack 1233000592, win 342, options [nop,nop,TS val 78908231 ecr 78908230], length 17210
11:52:36.491025 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394224477, win 3635, options [nop,nop,TS val 78908271 ecr 78908231], length 0
11:52:36.569355 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394224477:1394224487, ack 1233000592, win 342, options [nop,nop,TS val 78908350 ecr 78908271], length 10
11:52:36.609958 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394224487, win 3635, options [nop,nop,TS val 78908390 ecr 78908350], length 0
11:52:36.609993 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394224487:1394242657, ack 1233000592, win 342, options [nop,nop,TS val 78908390 ecr 78908390], length 18170
11:52:36.650181 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394242657, win 3635, options [nop,nop,TS val 78908430 ecr 78908390], length 0
11:52:36.731142 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394242657:1394242667, ack 1233000592, win 342, options [nop,nop,TS val 78908511 ecr 78908430], length 10
11:52:36.771958 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394242667, win 3635, options [nop,nop,TS val 78908552 ecr 78908511], length 0
11:52:36.771979 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394242667:1394258437, ack 1233000592, win 342, options [nop,nop,TS val 78908552 ecr 78908552], length 15770
11:52:36.811957 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394258437, win 3580, options [nop,nop,TS val 78908592 ecr 78908552], length 0
11:52:36.892064 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394258437:1394258447, ack 1233000592, win 342, options [nop,nop,TS val 78908672 ecr 78908592], length 10
11:52:36.932039 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394258447, win 3580, options [nop,nop,TS val 78908712 ecr 78908672], length 0
11:52:36.932082 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394258447:1394276017, ack 1233000592, win 342, options [nop,nop,TS val 78908712 ecr 78908712], length 17570
11:52:36.971983 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394276017, win 3499, options [nop,nop,TS val 78908752 ecr 78908712], length 0
11:52:37.056904 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394276017:1394276027, ack 1233000592, win 342, options [nop,nop,TS val 78908837 ecr 78908752], length 10
11:52:37.096966 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394276027, win 3499, options [nop,nop,TS val 78908877 ecr 78908837], length 0
11:52:37.096989 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394276027:1394291677, ack 1233000592, win 342, options [nop,nop,TS val 78908877 ecr 78908877], length 15650
11:52:37.136941 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394291677, win 3425, options [nop,nop,TS val 78908917 ecr 78908877], length 0
11:52:37.223275 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394291677:1394291687, ack 1233000592, win 342, options [nop,nop,TS val 78909004 ecr 78908917], length 10
11:52:37.263951 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394291687, win 3425, options [nop,nop,TS val 78909044 ecr 78909004], length 0
11:52:37.263974 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394291687:1394308477, ack 1233000592, win 342, options [nop,nop,TS val 78909044 ecr 78909044], length 16790
11:52:37.303956 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394308477, win 3347, options [nop,nop,TS val 78909084 ecr 78909044], length 0
11:52:37.383620 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394308477:1394308487, ack 1233000592, win 342, options [nop,nop,TS val 78909164 ecr 78909084], length 10
11:52:37.423926 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394308487, win 3347, options [nop,nop,TS val 78909204 ecr 78909164], length 0
11:52:37.423952 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394308487:1394326297, ack 1233000592, win 342, options [nop,nop,TS val 78909204 ecr 78909204], length 17810
11:52:37.463914 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394326297, win 3266, options [nop,nop,TS val 78909244 ecr 78909204], length 0
11:52:37.545414 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394326297:1394326307, ack 1233000592, win 342, options [nop,nop,TS val 78909326 ecr 78909244], length 10
11:52:37.586052 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394326307, win 3266, options [nop,nop,TS val 78909366 ecr 78909326], length 0
11:52:37.586109 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394326307:1394343097, ack 1233000592, win 342, options [nop,nop,TS val 78909366 ecr 78909366], length 16790
11:52:37.626049 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394343097, win 3188, options [nop,nop,TS val 78909406 ecr 78909366], length 0
11:52:37.707516 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394343097:1394343107, ack 1233000592, win 342, options [nop,nop,TS val 78909488 ecr 78909406], length 10
11:52:37.747870 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394343107, win 3188, options [nop,nop,TS val 78909528 ecr 78909488], length 0
11:52:37.747892 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394343107:1394358877, ack 1233000592, win 342, options [nop,nop,TS val 78909528 ecr 78909528], length 15770
11:52:37.787982 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358877, win 3114, options [nop,nop,TS val 78909568 ecr 78909528], length 0
11:52:38.068368 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358877:1394358887, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909568], length 10
11:52:38.068434 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358887, win 3114, options [nop,nop,TS val 78909849 ecr 78909849], length 0
11:52:38.068459 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358887:1394358897, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.068461 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358897, win 3114, options [nop,nop,TS val 78909849 ecr 78909849], length 0
11:52:38.068466 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358897:1394358907, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.068467 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358907, win 3114, options [nop,nop,TS val 78909849 ecr 78909849], length 0
11:52:38.068471 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358907:1394358917, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.068472 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358917, win 3114, options [nop,nop,TS val 78909849 ecr 78909849], length 0
11:52:38.068475 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358917:1394358927, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.068476 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358927, win 3114, options [nop,nop,TS val 78909849 ecr 78909849], length 0
11:52:38.068480 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358937, win 3114, options [nop,nop,TS val 78909849 ecr 78909849], length 0
11:52:38.068483 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358937:1394358947, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.068488 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358947:1394358957, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.068491 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358957:1394358967, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.068496 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358967:1394358977, ack 1233000592, win 342, options [nop,nop,TS val 78909849 ecr 78909849], length 10
11:52:38.108851 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394358977, win 3114, options [nop,nop,TS val 78909889 ecr 78909849], length 0
11:52:38.108910 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394358977:1394376697, ack 1233000592, win 342, options [nop,nop,TS val 78909889 ecr 78909889], length 17720
11:52:38.149011 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394376697, win 3032, options [nop,nop,TS val 78909929 ecr 78909889], length 0
11:52:38.234851 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394376697:1394376707, ack 1233000592, win 342, options [nop,nop,TS val 78910015 ecr 78909929], length 10
11:52:38.274934 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394376707, win 3032, options [nop,nop,TS val 78910055 ecr 78910015], length 0
11:52:38.274949 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394376707:1394392477, ack 1233000592, win 342, options [nop,nop,TS val 78910055 ecr 78910055], length 15770
11:52:38.314919 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394392477, win 2958, options [nop,nop,TS val 78910095 ecr 78910055], length 0
11:52:38.314940 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394392477:1394409277, ack 1233000592, win 342, options [nop,nop,TS val 78910095 ecr 78910095], length 16800
11:52:38.354895 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394409277, win 2889, options [nop,nop,TS val 78910135 ecr 78910095], length 0
11:52:38.354913 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394409277:1394426077, ack 1233000592, win 342, options [nop,nop,TS val 78910135 ecr 78910135], length 16800
11:52:38.394876 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394426077, win 2820, options [nop,nop,TS val 78910175 ecr 78910135], length 0
11:52:38.394890 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394426077:1394442877, ack 1233000592, win 342, options [nop,nop,TS val 78910175 ecr 78910175], length 16800
11:52:38.434909 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394442877, win 2751, options [nop,nop,TS val 78910215 ecr 78910175], length 0
11:52:38.434925 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394442877:1394459677, ack 1233000592, win 342, options [nop,nop,TS val 78910215 ecr 78910215], length 16800
11:52:38.474890 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394459677, win 2683, options [nop,nop,TS val 78910255 ecr 78910215], length 0
11:52:38.474901 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394459677:1394470177, ack 1233000592, win 342, options [nop,nop,TS val 78910255 ecr 78910255], length 10500
11:52:38.514891 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394470177, win 2638, options [nop,nop,TS val 78910295 ecr 78910255], length 0
11:52:38.514908 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394470177:1394476477, ack 1233000592, win 342, options [nop,nop,TS val 78910295 ecr 78910295], length 6300
11:52:38.554921 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394476477, win 2610, options [nop,nop,TS val 78910335 ecr 78910295], length 0
11:52:38.554941 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394476477:1394493277, ack 1233000592, win 342, options [nop,nop,TS val 78910335 ecr 78910335], length 16800
11:52:38.594888 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394493277, win 2541, options [nop,nop,TS val 78910375 ecr 78910335], length 0
11:52:38.594905 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394493277:1394510077, ack 1233000592, win 342, options [nop,nop,TS val 78910375 ecr 78910375], length 16800
11:52:38.634904 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394510077, win 2472, options [nop,nop,TS val 78910415 ecr 78910375], length 0
11:52:38.634922 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394510077:1394527897, ack 1233000592, win 342, options [nop,nop,TS val 78910415 ecr 78910415], length 17820
11:52:38.674852 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394527897, win 2400, options [nop,nop,TS val 78910455 ecr 78910415], length 0
11:52:38.674864 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394527897:1394544637, ack 1233000592, win 342, options [nop,nop,TS val 78910455 ecr 78910455], length 16740
11:52:38.714870 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394544637, win 2331, options [nop,nop,TS val 78910495 ecr 78910455], length 0
11:52:38.714897 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394544637:1394560337, ack 1233000592, win 342, options [nop,nop,TS val 78910495 ecr 78910495], length 15700
11:52:38.754893 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394560337, win 2266, options [nop,nop,TS val 78910535 ecr 78910495], length 0
11:52:38.754925 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394560337:1394560357, ack 1233000592, win 342, options [nop,nop,TS val 78910535 ecr 78910535], length 20
11:52:38.794922 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394560357, win 2266, options [nop,nop,TS val 78910575 ecr 78910535], length 0
11:52:38.794942 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394560357:1394577157, ack 1233000592, win 342, options [nop,nop,TS val 78910575 ecr 78910575], length 16800
11:52:38.834848 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394577157, win 2188, options [nop,nop,TS val 78910615 ecr 78910575], length 0
11:52:38.834868 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394577157:1394595037, ack 1233000592, win 342, options [nop,nop,TS val 78910615 ecr 78910615], length 17880
11:52:38.874858 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394595037, win 2115, options [nop,nop,TS val 78910655 ecr 78910615], length 0
11:52:38.874878 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394595037:1394610757, ack 1233000592, win 342, options [nop,nop,TS val 78910655 ecr 78910655], length 15720
11:52:38.914841 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394610757, win 2050, options [nop,nop,TS val 78910695 ecr 78910655], length 0
11:52:38.914854 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394610757:1394628637, ack 1233000592, win 342, options [nop,nop,TS val 78910695 ecr 78910695], length 17880
11:52:38.954812 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394628637, win 1977, options [nop,nop,TS val 78910735 ecr 78910695], length 0
11:52:38.954841 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394628637:1394645437, ack 1233000592, win 342, options [nop,nop,TS val 78910735 ecr 78910735], length 16800
11:52:38.994885 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394645437, win 1908, options [nop,nop,TS val 78910775 ecr 78910735], length 0
11:52:38.994895 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394645437:1394652257, ack 1233000592, win 342, options [nop,nop,TS val 78910775 ecr 78910775], length 6820
11:52:39.035093 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394652257, win 1878, options [nop,nop,TS val 78910815 ecr 78910775], length 0
11:52:39.035141 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394652257:1394662237, ack 1233000592, win 342, options [nop,nop,TS val 78910815 ecr 78910815], length 9980
11:52:39.074820 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394662237, win 1836, options [nop,nop,TS val 78910855 ecr 78910815], length 0
11:52:39.074842 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394662237:1394677717, ack 1233000592, win 342, options [nop,nop,TS val 78910855 ecr 78910855], length 15480
11:52:39.114860 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394677717, win 1771, options [nop,nop,TS val 78910895 ecr 78910855], length 0
11:52:39.114880 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394677717:1394694517, ack 1233000592, win 342, options [nop,nop,TS val 78910895 ecr 78910895], length 16800
11:52:39.154880 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394694517, win 1702, options [nop,nop,TS val 78910935 ecr 78910895], length 0
11:52:39.154897 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394694517:1394711317, ack 1233000592, win 342, options [nop,nop,TS val 78910935 ecr 78910935], length 16800
11:52:39.194830 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394711317, win 1633, options [nop,nop,TS val 78910975 ecr 78910935], length 0
11:52:39.194842 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394711317:1394728117, ack 1233000592, win 342, options [nop,nop,TS val 78910975 ecr 78910975], length 16800
11:52:39.234790 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394728117, win 1564, options [nop,nop,TS val 78911015 ecr 78910975], length 0
11:52:39.234826 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394728117:1394740847, ack 1233000592, win 342, options [nop,nop,TS val 78911015 ecr 78911015], length 12730
11:52:39.274844 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394740847, win 1511, options [nop,nop,TS val 78911055 ecr 78911015], length 0
11:52:39.274862 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394740847:1394744797, ack 1233000592, win 342, options [nop,nop,TS val 78911055 ecr 78911055], length 3950
11:52:39.314819 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394744797, win 1493, options [nop,nop,TS val 78911095 ecr 78911055], length 0
11:52:39.314841 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394744797:1394761597, ack 1233000592, win 342, options [nop,nop,TS val 78911095 ecr 78911095], length 16800
11:52:39.354806 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394761597, win 1424, options [nop,nop,TS val 78911135 ecr 78911095], length 0
11:52:39.354823 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394761597:1394778397, ack 1233000592, win 342, options [nop,nop,TS val 78911135 ecr 78911135], length 16800
11:52:39.394793 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394778397, win 1355, options [nop,nop,TS val 78911175 ecr 78911135], length 0
11:52:39.394814 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394778397:1394795197, ack 1233000592, win 342, options [nop,nop,TS val 78911175 ecr 78911175], length 16800
11:52:39.434759 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394795197, win 1286, options [nop,nop,TS val 78911215 ecr 78911175], length 0
11:52:39.434800 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394795197:1394811997, ack 1233000592, win 342, options [nop,nop,TS val 78911215 ecr 78911215], length 16800
11:52:39.474748 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394811997, win 1217, options [nop,nop,TS val 78911255 ecr 78911215], length 0
11:52:39.474760 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394811997:1394824597, ack 1233000592, win 342, options [nop,nop,TS val 78911255 ecr 78911255], length 12600
11:52:39.514771 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394824597, win 1164, options [nop,nop,TS val 78911295 ecr 78911255], length 0
11:52:39.514789 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394824597:1394828797, ack 1233000592, win 342, options [nop,nop,TS val 78911295 ecr 78911295], length 4200
11:52:39.554833 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394828797, win 1145, options [nop,nop,TS val 78911335 ecr 78911295], length 0
11:52:39.554853 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394828797:1394845597, ack 1233000592, win 342, options [nop,nop,TS val 78911335 ecr 78911335], length 16800
11:52:39.594805 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394845597, win 1076, options [nop,nop,TS val 78911375 ecr 78911335], length 0
11:52:39.594830 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394845597:1394862397, ack 1233000592, win 342, options [nop,nop,TS val 78911375 ecr 78911375], length 16800
11:52:39.634725 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394862397, win 1007, options [nop,nop,TS val 78911415 ecr 78911375], length 0
11:52:39.634744 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394862397:1394879197, ack 1233000592, win 342, options [nop,nop,TS val 78911415 ecr 78911415], length 16800
11:52:39.674776 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394879197, win 938, options [nop,nop,TS val 78911455 ecr 78911415], length 0
11:52:39.674789 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394879197:1394895997, ack 1233000592, win 342, options [nop,nop,TS val 78911455 ecr 78911455], length 16800
11:52:39.714767 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394895997, win 869, options [nop,nop,TS val 78911495 ecr 78911455], length 0
11:52:39.714876 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394895997:1394910697, ack 1233000592, win 342, options [nop,nop,TS val 78911495 ecr 78911495], length 14700
11:52:39.754771 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394910697, win 808, options [nop,nop,TS val 78911535 ecr 78911495], length 0
11:52:39.754788 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394910697:1394912797, ack 1233000592, win 342, options [nop,nop,TS val 78911535 ecr 78911535], length 2100
11:52:39.794797 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394912797, win 797, options [nop,nop,TS val 78911575 ecr 78911535], length 0
11:52:39.794817 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394912797:1394929597, ack 1233000592, win 342, options [nop,nop,TS val 78911575 ecr 78911575], length 16800
11:52:39.834832 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394929597, win 728, options [nop,nop,TS val 78911615 ecr 78911575], length 0
11:52:39.834853 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394929597:1394947417, ack 1233000592, win 342, options [nop,nop,TS val 78911615 ecr 78911615], length 17820
11:52:39.874741 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394947417, win 655, options [nop,nop,TS val 78911655 ecr 78911615], length 0
11:52:39.874759 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394947417:1394963437, ack 1233000592, win 342, options [nop,nop,TS val 78911655 ecr 78911655], length 16020
11:52:39.914743 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394963437, win 589, options [nop,nop,TS val 78911695 ecr 78911655], length 0
11:52:39.914756 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394963437:1394980117, ack 1233000592, win 342, options [nop,nop,TS val 78911695 ecr 78911695], length 16680
11:52:39.954818 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394980117, win 520, options [nop,nop,TS val 78911735 ecr 78911695], length 0
11:52:39.954830 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394980117:1394996917, ack 1233000592, win 342, options [nop,nop,TS val 78911735 ecr 78911735], length 16800
11:52:39.994802 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394996917, win 451, options [nop,nop,TS val 78911775 ecr 78911735], length 0
11:52:39.995152 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394996917:1394996927, ack 1233000592, win 342, options [nop,nop,TS val 78911775 ecr 78911775], length 10
11:52:40.035846 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394996927, win 451, options [nop,nop,TS val 78911816 ecr 78911775], length 0
11:52:40.035915 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394996927:1395013717, ack 1233000592, win 342, options [nop,nop,TS val 78911816 ecr 78911816], length 16790
11:52:40.075794 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395013717, win 374, options [nop,nop,TS val 78911856 ecr 78911816], length 0
11:52:40.075829 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1395013717:1395030517, ack 1233000592, win 342, options [nop,nop,TS val 78911856 ecr 78911856], length 16800
11:52:40.115847 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395030517, win 305, options [nop,nop,TS val 78911896 ecr 78911856], length 0
11:52:40.115866 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1395030517:1395047317, ack 1233000592, win 342, options [nop,nop,TS val 78911896 ecr 78911896], length 16800
11:52:40.155703 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395047317, win 174, options [nop,nop,TS val 78911936 ecr 78911896], length 0
11:52:40.155752 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1395047317:1395064117, ack 1233000592, win 342, options [nop,nop,TS val 78911936 ecr 78911936], length 16800
11:52:40.195132 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395064117, win 43, options [nop,nop,TS val 78911976 ecr 78911936], length 0
11:52:40.435748 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1395064117:1395069621, ack 1233000592, win 342, options [nop,nop,TS val 78912216 ecr 78911976], length 5504
11:52:40.435782 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395069621, win 0, options [nop,nop,TS val 78912216 ecr 78912216], length 0
11:52:40.670661 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78912451 ecr 78912216], length 0
11:52:40.670674 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395069621, win 0, options [nop,nop,TS val 78912451 ecr 78912216], length 0
11:52:41.141703 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78912922 ecr 78912451], length 0
11:52:42.083643 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78913864 ecr 78912451], length 0
11:52:42.083655 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395069621, win 0, options [nop,nop,TS val 78913864 ecr 78912216], length 0
11:52:43.967506 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78915748 ecr 78913864], length 0
11:52:43.967532 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395069621, win 0, options [nop,nop,TS val 78915748 ecr 78912216], length 0
11:52:47.739259 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78919520 ecr 78915748], length 0
11:52:47.739274 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395069621, win 0, options [nop,nop,TS val 78919520 ecr 78912216], length 0
11:52:55.275863 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78927056 ecr 78919520], length 0
11:52:55.275931 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1395069621, win 0, options [nop,nop,TS val 78927056 ecr 78912216], length 0
```

抓取到的前三个数据包是 **blocking_client** 与 **blocking_server** 建立三次握手的过程。

```
11:52:35.907381 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [S], seq 1394135076, win 43690, options [mss 65495,sackOK,TS val 78907688 ecr 0,nop,wscale 7], length 0
```

```
20:32:21.261484 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [S.], seq 1233000591, ack 1394135077, win 43690, options [mss 65495,sackOK,TS val 78907688 ecr 78907688,nop,wscale 7], length 0
```

```
11:52:35.907441 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
```

示意图如下：

![](http://www.hootina.org/github_easyserverdev/20181218121002.png)

当每次 **blocking_client** 给 **blocking_server** 发数据以后，**blocking_server** 会应答 **blocking_server**，在每次应答的数据包中会带上自己的当前可用 TCP 窗口大小（看上文中结果从 **127.0.0.1.3000 > 127.0.0.1.40846** 方向的数据包的 **win** 字段大小变化），由于 TCP 流量控制和拥赛控制机制的存在，**blocking_server** 端的 TCP 窗口大小短期内会慢慢增加，后面随着接收缓冲区中数据积压越来越多， TCP 窗口会慢慢变小，最终变为 0。

另外，细心的读者如果实际去做一下这个实验会发现一个现象，即当 tcpdump 已经显示对端的 TCP 窗口是 0 时， **blocking_client** 仍然可以继续发送一段时间的数据，此时的数据已经不是在发往对端，而是逐渐填满到本端的内核发送缓冲区中去了，这也验证了 send 函数实际上是往内核缓冲区中拷贝数据这一行为。



##### socket 非阻塞模式下的 send 行为

我们再来验证一下非阻塞 socket 的 send 行为，**server** 端的代码不变，我们将 **blocking_client.cpp** 中 socket 设置成非阻塞的，修改后的代码如下：

```
/**
 * 验证非阻塞模式下send函数的行为，client端，nonblocking_client.cpp
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
		int ret = send(clientfd, SEND_DATA, strlen(SEND_DATA), 0);
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
		if (ret == 0)
		{
			//对端关闭了连接，我们也关闭
			std::cout << "send data error." << std::endl;
			close(clientfd);
			break;
		} 
		else
		{
			count ++;
			std::cout << "send data successfully, count = " << count << std::endl;
		}
	}
	
	//5. 关闭socket
	close(clientfd);

    return 0;
}
```

编译 **nonblocking_client.cpp** 得到可执行程序 **nonblocking_client**：

```
 g++ -g -o nonblocking_client nonblocking_client.cpp 
```

运行 **nonblocking_client**，运行一段时间后，由于对端和本端的 TCP 窗口已满，数据发不出去了，但是 send 函数不会阻塞，而是立即返回，返回值是 **-1**（Windows 系统上 返回 SOCKET_ERROR，这个宏的值也是 **-1**），此时得到错误码是 **EWOULDBLOCK**。执行结果如下：

![](http://www.hootina.org/github_easyserverdev/20181218143641.png)



##### socket 阻塞模式下的 recv 行为

在了解了 send 函数的行为，我们再来看一下阻塞模式下的 recv 函数行为。服务器端代码不需要修改，我们修改一下客户端代码，如果服务器端不给客户端发数据，此时客户端调用 recv 函数执行流会阻塞在 recv 函数调用处。继续修改一下客户端代码：

```
/**
 * 验证阻塞模式下recv函数的行为，client端，blocking_client_recv.cpp
 * zhangyl 2018.12.17
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
        close(clientfd);
        return -1;
    }
	
	//直接调用recv函数，程序会阻塞在recv函数调用处
	char recvbuf[32] = {0};
	int ret = recv(clientfd, recvbuf, 32, 0);
	if (ret > 0) 
	{
		std::cout << "recv successfully." << std::endl;
	} 
	else 
	{
		std::cout << "recv data error." << std::endl;
	}
	
	//5. 关闭socket
	close(clientfd);

    return 0;
}
```

编译 **blocking_client_recv.cpp** 并使用启动，我们发现程序既没有打印 recv 调用成功的信息也没有调用失败的信息，将程序中断下来，使用 **bt** 命令查看此时的调用堆栈，发现程序确实阻塞在 recv 函数调用处。

```
[root@localhost testsocket]# g++ -g -o blocking_client_recv blocking_client_recv.cpp 
[root@localhost testsocket]# gdb blocking_client_recv
Reading symbols from /root/testsocket/blocking_client_recv...done.
(gdb) r
Starting program: /root/testsocket/blocking_client_recv 
^C
Program received signal SIGINT, Interrupt.
0x00007ffff72f119d in recv () from /lib64/libc.so.6
Missing separate debuginfos, use: debuginfo-install glibc-2.17-196.el7_4.2.x86_64 libgcc-4.8.5-16.el7_4.2.x86_64 libstdc++-4.8.5-16.el7_4.2.x86_64
(gdb) bt
#0  0x00007ffff72f119d in recv () from /lib64/libc.so.6
#1  0x0000000000400b18 in main (argc=1, argv=0x7fffffffe588) at blocking_client_recv.cpp:40
```



##### socket 非阻塞模式下的 recv 行为

非阻塞模式下如果当前无数据可读，recv 函数将立即返回，返回值为 **-1**，错误码为 **EWOULDBLOCK**。将客户端代码修成一下：

```
/**
 * 验证阻塞模式下recv函数的行为，client端，blocking_client_recv.cpp
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
	
	//直接调用recv函数，程序会阻塞在recv函数调用处
	while (true)
	{
		char recvbuf[32] = {0};
		int ret = recv(clientfd, recvbuf, 32, 0);
		if (ret > 0) 
		{
			//收到了数据
			std::cout << "recv successfully." << std::endl;
		} 
		else if (ret == 0)
		{
			//对端关闭了连接
			std::cout << "peer close the socket." << std::endl;	
			break;
		} 
		else if (ret == -1) 
		{
			if (errno == EWOULDBLOCK)
			{
				std::cout << "There is no data available now." << std::endl;
			} 
			else if (errno == EINTR) 
			{
				//如果被信号中断了，则继续重试recv函数
				std::cout << "recv data interrupted by signal." << std::endl;				
			} else
			{
				//真的出错了
				break;
			}
		}
	}
	
	//5. 关闭socket
	close(clientfd);

    return 0;
}
```



执行结果与我们预期的一模一样， recv 函数在无数据可读的情况下并不会阻塞情绪，所以程序会一直有“**There is no data available now.**”相关的输出。

![](http://www.hootina.org/github_easyserverdev/20181218151618.png)



------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)