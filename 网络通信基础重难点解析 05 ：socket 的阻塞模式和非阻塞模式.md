## 网络通信基础重难点解析 05 ：socket 的阻塞模式和非阻塞模式



### 4.5 socket 的阻塞模式和非阻塞模式

对 socket 在阻塞和非阻塞模式下的各个函数的行为差别深入的理解是掌握网络编程的基本要求之一，是重点也是难点。

阻塞和非阻塞模式下，我们常讨论的具有不同行为表现的 socket 函数一般有如下几个，见下表：

- connect
- accept
- send (Linux 平台上对 socket 进行操作时也包括 **write** 函数，下文中对 send 函数的讨论也适用于 **write** 函数)
- recv (Linux 平台上对 socket 进行操作时也包括 **read** 函数，下文中对 recv 函数的讨论也适用于 **read** 函数)



在正式讨论以上四个函数之前，我们先解释一下阻塞模式和非阻塞模式的概念。所谓**阻塞模式**，**就当某个函数“执行成功的条件”当前不能满足时，该函数会阻塞当前执行线程，程序执行流在超时时间到达或“执行成功的条件”满足后恢复继续执行**。而**非阻塞模式**恰恰相反，即使某个函数的“执行成功的条件”不当前不能满足，该函数也不会阻塞当前执行线程，而是立即返回，继续运行执行程序流。如果读者不太明白这两个定义也没关系，后面我们会以具体的示例来讲解这两种模式的区别。

#### 如何将 socket 设置成非阻塞模式

无论是 Windows 还是 Linux 平台，默认创建的 socket 都是阻塞模式的。

在 Linux 平台上，我们可以使用 **fcntl() 函数**或 **ioctl() 函数**给创建的 socket 增加 **O_NONBLOCK** 标志来将 socket 设置成非阻塞模式。示例代码如下：

```
int oldSocketFlag = fcntl(sockfd, F_GETFL, 0);
int newSocketFlag = oldSocketFlag | O_NONBLOCK;
fcntl(sockfd, F_SETFL,  newSocketFlag);
```

**ioctl() 函数** 与 **fcntl()** 函数使用方式基本一致，这里就不再给出示例代码了。

当然，Linux 下的 **socket()** 创建函数也可以直接在创建时将 socket 设置为非阻塞模式，**socket()** 函数的签名如下：

```
int socket(int domain, int type, int protocol);
```

给 **type** 参数增加一个 **SOCK_NONBLOCK** 标志即可，例如：

```
int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
```

不仅如此，Linux 系统下利用 accept() 函数返回的代表与客户端通信的 socket 也提供了一个扩展函数 **accept4()**，直接将 accept 函数返回的 socket 设置成非阻塞的。

```
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen); 
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
```

只要将 **accept4()** 函数最后一个参数 **flags** 设置成 **SOCK_NONBLOCK** 即可。也就是说以下代码是等价的：

```
socklen_t addrlen = sizeof(clientaddr);
int clientfd = accept4(listenfd, &clientaddr, &addrlen, SOCK_NONBLOCK);
```

```
socklen_t addrlen = sizeof(clientaddr);
int clientfd = accept(listenfd, &clientaddr, &addrlen);
if (clientfd != -1)
{
    int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
	int newSocketFlag = oldSocketFlag | O_NONBLOCK;
	fcntl(clientfd, F_SETFL,  newSocketFlag);
}
```



在 Windows 平台上，可以调用 **ioctlsocket() 函数** 将 socket 设置成非阻塞模式，**ioctlsocket()** 签名如下：

```
int ioctlsocket(SOCKET s, long cmd, u_long *argp);
```

将 **cmd** 参数设置为 **FIONBIO**，***argp** 设置为 **0** 即可将 socket 设置成阻塞模式，而将 ***argp** 设置成非 **0** 即可设置成非阻塞模式。示例如下：

```
//将 socket 设置成非阻塞模式
u_long argp = 1;
ioctlsocket(s, FIONBIO, &argp);

//将 socket 设置成阻塞模式
u_long argp = 0;
ioctlsocket(s, FIONBIO, &argp);
```

Windows 平台需要注意一个地方，如果对一个 socket 调用了 **WSAAsyncSelect()** 或 **WSAEventSelect()** 函数后，再调用 **ioctlsocket()** 函数将该 socket 设置为非阻塞模式会失败，你必须先调用 **WSAAsyncSelect()** 通过将 **lEvent** 参数为 **0** 或调用 **WSAEventSelect()** 通过设置 **lNetworkEvents** 参数为 **0** 来清除已经设置的 socket 相关标志位，再次调用 **ioctlsocket()** 将该 socket 设置成阻塞模式才会成功。因为调用 **WSAAsyncSelect()** 或**WSAEventSelect()** 函数会自动将 socket 设置成非阻塞模式。MSDN 上原文（https://docs.microsoft.com/en-us/windows/desktop/api/winsock/nf-winsock-ioctlsocket）如下：

```
The WSAAsyncSelect and WSAEventSelect functions automatically set a socket to nonblocking mode. If WSAAsyncSelect or WSAEventSelect has been issued on a socket, then any attempt to use ioctlsocket to set the socket back to blocking mode will fail with WSAEINVAL.

To set the socket back to blocking mode, an application must first disable WSAAsyncSelect by calling WSAAsyncSelect with the lEvent parameter equal to zero, or disable WSAEventSelect by calling WSAEventSelect with the lNetworkEvents parameter equal to zero.
```



关于 **WSAAsyncSelect()** 和 **WSAEventSelect()** 这两个函数，后文中会详细讲解。



> 注意事项：无论是 Linux 的 fcntl 函数，还是 Windows 的 ioctlsocket，建议读者在实际编码中判断一下函数返回值以确定是否调用成功。





------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)