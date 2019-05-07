## 网络通信基础重难点解析 13 ：Windows WSAEventSelect 网络通信模型



### Windows WSAEventSelect 网络通信模型

**WSAEventSelect** 网络通信模型是 Windows 系统上常用的一种异步 socket 通信模型，下面来详细介绍下其用法。

#### WSAEventSelect 用于服务器端

我们先从服务器端来看这个模型，在 Windows 系统上正常的一个服务器端 socket 通信流程是先初始化套接字库，然后创建侦听 socket，接着绑定 ip 地址和端口，再调用 **listen** 函数开启侦听。代码如下：

```
	//1. 初始化套接字库
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);
    int nError = WSAStartup(wVersionRequested, &wsaData);
    if (nError != 0)
        return -1;
   
    if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
    {
        WSACleanup();
        return -1;
    }

    //2. 创建用于监听的套接字
    SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN addrSrv;
    addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(6000);
    
    //3. 绑定套接字
    if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        closesocket(sockSrv);
        WSACleanup();
        return -1;
    }
    
    
    //4. 将套接字设为监听模式，准备接受客户请求
    if (listen(sockSrv, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(sockSrv);
        WSACleanup();
        return -1;
    }
```



正常的流程下接着是等待客户端连接，然后调用 **accept** 接受客户端连接。在这里，我们使用 **WSAEventSelect** 函数给侦听 socket 设置需要关注的事件。**WSAEventSelect** 的函数如下：

```
int WSAAPI WSAEventSelect(
      SOCKET   s,
      WSAEVENT hEventObject,
      long     lNetworkEvents
);
```

- 参数 **s** 是需要操作的 socket 句柄；

- 参数 **hEventObject** 是需要与 socket 关联的内核事件对象，可以使用 **WSACreateEvent** 函数创建：

  ```
  WSAEVENT WSAAPI WSACreateEvent();
  ```

  **WSAEVENT** 类型本质上就是使用 **CreateEvent** 创建的 Event 对象：

  ```
  #define WSAEVENT HANDLE
  ```

- 参数 **lNetworkEvents** 是 socket 上需要关注的事件，常用的事件类型有：

  |   事件宏   |            事件含义            |
  | :--------: | :----------------------------: |
  |  FD_READ   |          socket 可读           |
  |  FD_WRITE  |          socket 可写           |
  | FD_ACCEPT  |      侦听 socket 有新连接      |
  | FD_CONNECT | 普通 socket 连接服务器得到响应 |
  |  FD_CLOSE  |            连接关闭            |

- **返回值**：**WSAEventSelect** 函数调用成功返回 0，调用失败返回 SOCKET_ERROR（-1）。



由于我们这里是侦听 socket，所以我们关注的事件是 **FD_ACCEPT**，代码如下：

```
WSAEVENT hListenEvent = WSACreateEvent();
if (WSAEventSelect(sockSrv, hListenEvent, FD_ACCEPT) == SOCKET_ERROR)
{
    WSACloseEvent(hListenEvent);
    closesocket(sockSrv);
    WSACleanup();
    return -1;
}
```

当 socket 上有我们关注的事件时，操作系统会让 **hListenEvent** 对象受信，所以接着我们使用 **WSAWaitForMultipleEvents** 函数去等待 **hListenEvent** 是否有信号，**WSAWaitForMultipleEvents** 签名如下：

```
DWORD WSAAPI WSAWaitForMultipleEvents(
    DWORD          	cEvents,
    const WSAEVENT* lphEvents,
    BOOL           	fWaitAll,
    DWORD          	dwTimeout,
    BOOL           	fAlertable
);
```

这个函数的使用方法和 **WaitForMultipleObjects** 一模一样，我们在第三章介绍过了，这里不再介绍。

调用 **WSAWaitForMultipleEvents** 示例代码如下：

```
WSAEVENT hEvents[1];
hEvents[0] = hListenEvent;
DWORD dwResult = WSAWaitForMultipleEvents(1, hEvents, FALSE, WSA_INFINITE, FALSE);

DWORD dwIndex = dwResult - WSA_WAIT_EVENT_0;
for (DWORD i = 0; i < dwIndex; ++i)
{
	//通过dwIndex编号找到hEvents数组中的WSAEvent对象，进而找到对应的socket 
}
```

通过 **dwIndex** 编号找到 **hEvents** 数组中的 **WSAEvent** 对象，进而找到对应的 socket，然后对这个 socket 调用 **WSAEnumNetworkEvents** 函数来获取该 socket 上的事件类型，**WSAEnumNetworkEvents** 函数签名如下：

```
int WSAAPI WSAEnumNetworkEvents(
    SOCKET             s,
    WSAEVENT           hEventObject,
    LPWSANETWORKEVENTS lpNetworkEvents
);
```

参数 **lpNetworkEvents** 是一个输出参数，其类型是 **WSANETWORKEVENTS** 结构体指针，其定义如下：

```
typedef struct _WSANETWORKEVENTS {
       long lNetworkEvents;
       int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS, *LPWSANETWORKEVENTS;
```

在调用 **WSAEnumNetworkEvents** 后我们就能通过 **lNetworkEvents** 类型得到对应的 socket 的事件类型，通过 **iErrorCode** 字段数组中的某一位确定该类型的事件是否有错误（**0** 值表示没有错误，**非 0** 值表示存在错误），与 **FD_XXX** 相对应，**iErrorCode** 每个下标都有确定的含义，下标值都被定义成了相应的宏，常见的有：

```
/*
 * WinSock 2 extension -- bit values and indices for FD_XXX network events
 */
#define FD_READ_BIT      0
#define FD_READ          (1 << FD_READ_BIT)

#define FD_WRITE_BIT     1
#define FD_WRITE         (1 << FD_WRITE_BIT)

#define FD_OOB_BIT       2
#define FD_OOB           (1 << FD_OOB_BIT)

#define FD_ACCEPT_BIT    3
#define FD_ACCEPT        (1 << FD_ACCEPT_BIT)

#define FD_CONNECT_BIT   4
#define FD_CONNECT       (1 << FD_CONNECT_BIT)

#define FD_CLOSE_BIT     5
#define FD_CLOSE         (1 << FD_CLOSE_BIT)

#define FD_QOS_BIT       6
#define FD_QOS           (1 << FD_QOS_BIT)

#define FD_GROUP_QOS_BIT 7
#define FD_GROUP_QOS     (1 << FD_GROUP_QOS_BIT)

#define FD_ROUTING_INTERFACE_CHANGE_BIT 8
#define FD_ROUTING_INTERFACE_CHANGE     (1 << FD_ROUTING_INTERFACE_CHANGE_BIT)

#define FD_ADDRESS_LIST_CHANGE_BIT 9
#define FD_ADDRESS_LIST_CHANGE     (1 << FD_ADDRESS_LIST_CHANGE_BIT)

#define FD_MAX_EVENTS    10
#define FD_ALL_EVENTS    ((1 << FD_MAX_EVENTS) - 1)
```



**WSAEnumNetworkEvents** 函数使用示例代码如下：

```
WSANETWORKEVENTS  triggeredEvents;
if (WSAEnumNetworkEvents(sockSrv, hEvents[dwIndex], &triggeredEvents) != SOCKET_ERROR)
{
    if (triggeredEvents.lNetworkEvents & FD_ACCEPT)
    {
    	// 0 值表示无错误
    	if (triggeredEvents.iErrorCode[FD_ACCEPT_BIT] == 0)
    	{
            //TODO：在这里可以调用accept函数处理接受连接事件。
    	}                    	
    }
}
```

上述代码第 **9** 行我们可以调用 **accept** 函数接受新连接，然后将新产生的 **clientsocket** 设置监听 FD_READ 和 FD_CLOSE 等事件。完整的代码如下所示：

```
/**
 * WSAEventSelect 模型演示
 * zhangyl 2019.03.16
 */
#include "stdafx.h"
#include <winsock2.h>
#include <stdio.h>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

int main(int argc, _TCHAR* argv[])
{
    //1. 初始化套接字库
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);
    int nError = WSAStartup(wVersionRequested, &wsaData);
    if (nError != 0)
        return -1;
   
    if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
    {
        WSACleanup();
        return -1;
    }

    //2. 创建用于监听的套接字
    SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN addrSrv;
    addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(6000);
    
    //3. 绑定套接字
    if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        closesocket(sockSrv);
        WSACleanup();
        return -1;
    }
    
    //4. 将套接字设为监听模式，准备接受客户请求
    if (listen(sockSrv, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(sockSrv);
        WSACleanup();
        return -1;
    }

    WSAEVENT hListenEvent = WSACreateEvent();
    if (WSAEventSelect(sockSrv, hListenEvent, FD_ACCEPT) == SOCKET_ERROR)
    {
        WSACloseEvent(hListenEvent);
        closesocket(sockSrv);
        WSACleanup();
        return -1;
    }
        

    WSAEVENT* pEvents = new WSAEVENT[1];
    pEvents[0] = hListenEvent;
    SOCKET* pSockets = new SOCKET[1];
    pSockets[0] = sockSrv;
    DWORD dwCount = 1;
    bool bNeedToMove;

    while (true)
    {
        bNeedToMove = false;
        DWORD dwResult = WSAWaitForMultipleEvents(dwCount, pEvents, FALSE, WSA_INFINITE, FALSE);
        if (dwResult == WSA_WAIT_FAILED)
            continue;

        DWORD dwIndex = dwResult - WSA_WAIT_EVENT_0;
        for (DWORD i = 0; i <= dwIndex; ++i)
        {
            //通过dwIndex编号找到hEvents数组中的WSAEvent对象，进而找到对应的socket
            WSANETWORKEVENTS  triggeredEvents;
            if (WSAEnumNetworkEvents(pSockets[i], pEvents[i], &triggeredEvents) == SOCKET_ERROR)
                continue;

            if (triggeredEvents.lNetworkEvents & FD_ACCEPT)
            {
                if (triggeredEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
                    continue;

                //调用accept函数处理接受连接事件;
                SOCKADDR_IN addrClient;
                int len = sizeof(SOCKADDR);
                //等待客户请求到来
                SOCKET hSockClient = accept(sockSrv, (SOCKADDR*)&addrClient, &len);
                if (hSockClient != SOCKET_ERROR)
                {
                    //监听客户端socket的可读和关闭事件
                    WSAEVENT hClientEvent = WSACreateEvent();
                    if (WSAEventSelect(hSockClient, hClientEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR)
                    {
                        WSACloseEvent(hClientEvent);
                        closesocket(hSockClient); 
                        continue;
                    }
                        
                    WSAEVENT* pEvents2 = new WSAEVENT[dwCount + 1];
                    SOCKET* pSockets2 = new SOCKET[dwCount + 1];
                    memcpy(pEvents2, pEvents, dwCount * sizeof(WSAEVENT));
                    pEvents2[dwCount] = hClientEvent;
                    memcpy(pSockets2, pSockets, dwCount * sizeof(SOCKET));
                    pSockets2[dwCount] = hSockClient;
                    delete[] pEvents;
                    delete[] pSockets;
                    pEvents = pEvents2;
                    pSockets = pSockets2;

                    dwCount++;

                    printf("a client connected, socket: %d, current: %d\n", (int)hSockClient, dwCount - 1);
                }
            }
            else if (triggeredEvents.lNetworkEvents & FD_READ)
            {
                if (triggeredEvents.iErrorCode[FD_READ_BIT] != 0)
                    continue;

                char szBuf[64] = { 0 };
                int nRet = recv(pSockets[i], szBuf, 64, 0);
                if (nRet > 0)
                {
                    printf("recv data: %s, client: %d\n", szBuf, pSockets[i]);
                }
            }
            else if (triggeredEvents.lNetworkEvents & FD_CLOSE)
            {
                //此处不要判断
                //if (triggeredEvents.iErrorCode[FD_READ_BIT] != 0)
                //    continue;
                
                printf("a client disconnected, socket: %d, current: %d\n", (int)pSockets[i], dwCount - 2);

                WSACloseEvent(pEvents[i]);
                closesocket(pSockets[i]);

                //标记为无效，循环结束后统一移除
                pSockets[i] = INVALID_SOCKET;

                bNeedToMove = true;
            }
            
        }// end for-loop

        if (bNeedToMove)
        {
            //移除无效的事件
            std::vector<SOCKET> vValidSockets;
            std::vector<HANDLE> vValidEvents;
            for (size_t i = 0; i < dwCount; ++i)
            {
                if (pSockets[i] != INVALID_SOCKET)
                {
                    vValidSockets.push_back(pSockets[i]);
                    vValidEvents.push_back(pEvents[i]);
                }
            }

            size_t validSize = vValidSockets.size();
            if (validSize > 0)
            {
                WSAEVENT* pEvents2 = new WSAEVENT[validSize];
                SOCKET* pSockets2 = new SOCKET[validSize];
                memcpy(pEvents2, &vValidEvents[0], validSize * sizeof(WSAEVENT));
                memcpy(pSockets2, &vValidSockets[0], validSize * sizeof(SOCKET));
                delete[] pEvents;
                delete[] pSockets;
                pEvents = pEvents2;
                pSockets = pSockets2;

                dwCount = validSize;
            }
        }
        
    }// end while-loop

    closesocket(sockSrv);

    WSACleanup();
                     
    return 0;
}
```

在 Visual Studio 2013 中编译该程序并运行，然后使用 Linux nc 命令模拟几个客户端连接该程序，效果如下所示：

![](http://www.hootina.org/github_easyserverdev/20190316222936.png)





------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)