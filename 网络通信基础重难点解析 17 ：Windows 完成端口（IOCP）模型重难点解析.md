## 网络通信基础重难点解析 17 ：Windows 完成端口（IOCP）模型重难点解析



### Windows 完成端口（IOCP）模型重难点解析

本人很多年前接触完成端口以来，期间学习和练习了很多次，本以为自己真正地理解了其原理，最近在看网狐的服务器端源码时又再一次拾起完成端口的知识，结果发现以前理解的其实很多偏差，有些理解的甚至都是错误的。网络上关于windows完成端口的介绍举不胜举，但大多数都是介绍怎么做，而不是为告诉读者为什么这么做。看了很多遍小猪的讲解：<http://blog.csdn.net/piggyxp/article/details/6922277>，终于有些顿悟。为自己也为别人，在这里做个备忘。

这篇文章将从为什么这么做的角度来解释完成端口的一些重难点。



#### 使用完成端口的基本步骤

使用完成端口一般按以下步骤（这里以网络服务器接受客户端连接并与客户端进行网络通信为例）：

```cpp
//步骤1：创建完成端口

//步骤2：创建侦听socket并将侦听socket绑定到完成端口上

//步骤3：设置侦听
```



**步骤 1 代码**：

```cpp
m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
```



**步骤 2 代码**：

```cpp
//创建侦听socket
m_pListenContext->m_Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 
                                       WSA_FLAG_OVERLAPPED);
// 将Listen Socket绑定至完成端口中
if( NULL== CreateIoCompletionPort((HANDLE)m_pListenContext->m_Socket, 		
                                  m_hIOCompletionPort,
                                  (ULONG_PTR)m_pListenContext,
                                  0))  
{  
	return false;
}
return true;
```



> 注意：必须使用 WSASocket 函数，并设置标识位 WSA_FLAG_OVERLAPPED。



**步骤 3 代码**：

```cpp
// 服务器地址信息，用于绑定Socket
struct sockaddr_in ServerAddress;
// 填充地址信息
ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
ServerAddress.sin_family = AF_INET;
// 这里可以绑定任何可用的IP地址，或者绑定一个指定的IP地址 
ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);                      
//ServerAddress.sin_addr.s_addr = inet_addr(CStringA(m_strIP).GetString());         
ServerAddress.sin_port = htons(m_nPort);                          

// 绑定地址和端口
if (SOCKET_ERROR == bind(m_pListenContext->m_Socket, 
                         (struct sockaddr *) &ServerAddress,
                         sizeof(ServerAddress))) 
	return false;

// 开始进行监听
if (SOCKET_ERROR == listen(m_pListenContext->m_Socket,SOMAXCONN))
	return false;

return true;
```



以上步骤都是完成端口约定俗成的套路，现在接下来的问题是如何接受客户端连接？



#### 完成端口重难点一： 使用 AcceptEx 代替 accept 时，完成端口模型让操作系统替我们接受新连接



不管是使用 select 函数还是 Linux 平台的 epoll 模型无非都是检测到侦听 socket 可读，然后再调用 accept 函数接受连接，这样存在一个问题，就是侦听 socket 只有一个，所以调用 accept 函数接受连接的逻辑也只能有一个（一般不会在多线程里面对同一个 socket 进行同一种操作）。但是如果是这样的话，如果同一时间有大量的连接来了，可能就要逐个接受连接了，相当于一群人排队进入一个门里面，那有没有更好的方法呢？有，Windows 系统提供了一个 **AcceptEx** 函数，在创建完侦听函数之后，调用这个函数，那么将来在完成端口的工作线程里面如果有接受新连接动作，则无需调用 accept 或者 AcceptEx，操作系统自动帮你接受新连接，等在工作线程里面得到通知的时候，连接已经建立，而且新的客户端socket也已经创建好。也就是说，如果使用 accept，不仅需要使用 accept 接受新连接，同时需要在连接现场建立一个 socket，而使用 **AcceptEx**，这两个步骤都不需要了。这是使用完成端口的一个优势之一。**AcceptEx** 函数签名如下：

```cpp
BOOL AcceptEx(
  _In_  SOCKET       sListenSocket,
  _In_  SOCKET       sAcceptSocket,
  _In_  PVOID        lpOutputBuffer,
  _In_  DWORD        dwReceiveDataLength,
  _In_  DWORD        dwLocalAddressLength,
  _In_  DWORD        dwRemoteAddressLength,
  _Out_ LPDWORD      lpdwBytesReceived,
  _In_  LPOVERLAPPED lpOverlapped
);
```

注意看第二个参数 **sAcceptSocket**，这个 socket 我们在初始化的时候需要准备好，将来新连接成功以后，可以直接使用这个 socket 表示客户端连接。但是你可能又会问，我初始化阶段需要准备多少个这样的 socke t呢？毕竟不可能多个连接使用同一个 sAcceptSocket。的确如此，所以一般初始化的时候准备一批客户端 socket，等工作线程有新连接成功后，表明开始准备的某个客户端socket已经被使用了，这个时候我们可以继续补充一个。相当于，我们预先准备五个容器，在使用过程中每次使用一个，我们就立刻补充一个。当然，**AcceptEx** 这个函数不仅准备了接受连接操作，同时也准备了连接的两端的地址缓冲区和对端发来的第一组数据缓冲区，将来有新连接成功以后，操作系统通知我们的时候，操作系统不仅帮我门接收好了连接，还将连接两端的地址和对端发过来的第一组数据填到我们指定的缓冲区了。

需要注意的是：对于 **AcceptEx**这个函数，MSDN 的建议这个函数最好不要直接使用，而是通过相应 API  获取该函数的指针，再调用之（<https://msdn.microsoft.com/en-us/library/windows/desktop/ms737524(v=vs.85).aspx>）：

```
**Note**  The function pointer for the AcceptEx function must be obtained at run time by making a call to the WSAIoctl function with the SIO_GET_EXTENSION_FUNCTION_POINTER opcode specified. The input buffer passed to the WSAIoctl function must contain WSAID_ACCEPTEX, a globally unique identifier (GUID) whose value identifies the AcceptEx extension function. On success, the output returned by the WSAIoctl function contains a pointer to the AcceptEx function. The WSAID_ACCEPTEX GUID is defined in the Mswsock.h header file.
```

代码应该写成这样：

```cpp
// 使用AcceptEx函数，因为这个是属于WinSock2规范之外的微软另外提供的扩展函数
// 所以需要额外获取一下函数的指针，
// 获取AcceptEx函数指针
DWORD dwBytes = 0;  
if(SOCKET_ERROR == WSAIoctl(m_pListenContext->m_Socket, 
                            SIO_GET_EXTENSION_FUNCTION_POINTER, 
                            &GuidAcceptEx, 
                            sizeof(GuidAcceptEx), 
                            &m_lpfnAcceptEx, 
                            sizeof(m_lpfnAcceptEx), 
                            &dwBytes, 
                            NULL, 
                            NULL))  
{  
	this->_ShowMessage(_T("WSAIoctl 未能获取AcceptEx函数指针。错误代码: %d\n"), 
                       WSAGetLastError());
	return false;  
}
```

**WSAIoctl** 函数第一个参数只要填写任意一个有效的 socket 就可以了。





#### 完成端口重难点二：完成端口模型让操作系统替我们进行数据收发

**NO1.** 写过网络通信程序的人都知道，尤其是服务器端程序，对于阻塞模式下的 socket， 我们一般不直接调用 send 或 recv 这类函数进行数据收发，因为当 TCP 窗口太小时，数据发不出去，send 会阻塞线程，同理，如果当前网络缓冲区没有数据，调用 recv 也会阻塞线程。当然这也是入门级的做法。

**NO2.** 既然上述做法不好，那我就换成主动检测数据是否可以收发，当数据可以收发的时候，再调用 send 或 recv 函数进行收发。这就是常用的IO复用函数的用途，如 select 函数、Linux 操作系统下的 poll 函数。这是中级做法。

**NO3.** 使用 IO 复用技术主动检测数据是否可读可写，也存在问题。如果检测到了数据可读或可写，那这种检测就是值得的；但是反之检测不到呢？那也是白白地浪费时间的。如果有一种方法，我不需要主动去检测，我只需要预先做一个部署，当有数据可读或者可写时，操作系统能通知我就好了，而不是每次都是我自己去主动检测。有，这就是 Linux 下的 epoll 模型和 Windows 下的 **WSAAsyncSelect** 和完成端口模型。这是高级做法。

**NO4.** 但是无论是 epoll 模型还是 WSAAsyncSelect 模型，虽然操作系统会告诉我们什么时候数据可读或者可写，但是当数据可读或者可写时，还是需要我们自己去调用 send 或者 recv 函数做实际的收发数据工作。那有没有一种模型，不仅能通知我们数据可读和可写，甚至当数据可读或者可写时，连数据的收发工作也帮我们做好了？有，这就是 Windows 的完成端口模型。

这就是标题所说的完成端口将 IO 操作从手动变为自动，完成端口将数据的可读与可写检测操作和收发数据操作这两项工作改为操作系统代劳，等系统完成之后会通知我们的，而我们只需要在这之前做一些相应的部署（初始化工作）就可以了。   那么需要做那些初始化工作呢？这里我们以收发网络数据为例。



- **对于收数据，我们只需要准备好存放数据的缓冲区就可以了**：

```cpp
// 初始化变量
DWORD dwFlags = 0;
DWORD dwBytes = 0;
WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

pIoContext->ResetBuffer();
pIoContext->m_OpType = RECV_POSTED;

// 初始化完成后，，投递WSARecv请求
int nBytesRecv = WSARecv(pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
{
	this->_ShowMessage(_T("投递第一个WSARecv失败！"));
	return false;
}
```

上述代码中 **WSARecv** 函数会立刻返回，不会阻塞，如果返回时数据已经收成功了，那我们准备的缓冲区 **m_wsaBuf** 中存放的就是我们收到的数据；否则 **WASRecv**会返回 **-1**（对应 SOCKET_ERROR 宏），此时错误码如果是 **WSA_IO_PENDING** 表示收数据暂且还没完成，这样你需要等待后续通知。所以从某种意义上来说**WSARecv 函数并不是收取数据，而更像是安排让操作系统收数据的设置**，这种行为术语叫**投递一个收数据的请求**。



- 同理，**对于发数据，我们也只要准备好需要发送的数据即可**：

```cpp
// 初始化变量
DWORD dwFlags = 0;
DWORD dwBytes = 0;
WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

pIoContext->ResetBuffer();
pIoContext->m_OpType = SEND_POSTED;

// 初始化完成后，，投递WSARecv请求
int nBytesSend = WSASend(pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
if ((SOCKET_ERROR == nBytesSend) && (WSA_IO_PENDING != WSAGetLastError()))
{
	this->_ShowMessage(_T("发送数据失败！"));
	return false;
}
```

![点击并拖拽以移动](data:image/gif;base64,R0lGODlhAQABAPABAP///wAAACH5BAEKAAAALAAAAAABAAEAAAICRAEAOw==)

发数据的代码基本上和收数据一模一样，**WSASend** 函数也不是实际发送数据，而是**投递一个发数据的请求**。因此也会立即返回，如果其返回值是 **-1**（对应 SOCKET_ERROR 宏），且错误码不是 **WSA_IO_PENDING**，则表示调用真的出错了；如果错误码是 **WSA_IO_PENDING** 表示发送数据正在进行中，当前还没完成。



#### 完成端口完整流程示例

上面介绍了一些不成体系的代码片段，那么我们应该怎么把上面介绍的代码组织成一个整体呢？完成端口模型，需要初始化步骤中还需要建立一些工作线程，这些工作线程就是用来处理各种操作系统的通知的，比如有新客户端连接成功了、数据收好了、数据发送好了等等。创建工作线程以及准备新连接到来时需要的一些容器的代码（上文介绍过了，如需要提前准备的一些 acceptSocket、两端地址缓冲区、第一份收到的数据缓冲区）：

**创建工作线程**：

```cpp
DWORD nThreadID;
for (int i = 0; i < m_nThreads; i++)
{
	THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
	pThreadParams->nThreadNo  = i+1;
	m_phWorkerThreads[i] = ::CreateThread(0, 0, _WorkerThread, (void *)pThreadParams, 
                                          0, &nThreadID);
}
```



**调用 AcceptEx 为将来接受新连接准备**：

```cpp
// 为AcceptEx准备参数，然后投递AcceptEx I/O请求
for( int i=0;i<MAX_POST_ACCEPT;i++ )
{
	// 新建一个IO_CONTEXT
	PER_IO_CONTEXT* pAcceptIoContext = m_pListenContext->GetNewIoContext();

	if( false==this->_PostAccept( pAcceptIoContext ) )
	{
		m_pListenContext->RemoveContext(pAcceptIoContext);
		return false;
	}
}

// 投递Accept请求
bool CIOCPModel::_PostAccept( PER_IO_CONTEXT* pAcceptIoContext )
{
	ASSERT( INVALID_SOCKET!=m_pListenContext->m_Socket );

	// 准备参数
	DWORD dwBytes = 0;  
	pAcceptIoContext->m_OpType = ACCEPT_POSTED;  
	WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;
	
	// 为以后新连入的客户端先准备好Socket( 这个是与传统accept最大的区别 ) 
	pAcceptIoContext->m_sockAccept  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  
	if( INVALID_SOCKET==pAcceptIoContext->m_sockAccept )  
	{  
        _ShowMessage(_T("创建用于Accept的Socket失败！错误代码: %d"), WSAGetLastError());
		return false;  
	} 

	// 投递AcceptEx
	if(FALSE == m_lpfnAcceptEx( m_pListenContext->m_Socket, pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),   
								sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))  
	{  
		if(WSA_IO_PENDING != WSAGetLastError())  
		{  
            _ShowMessage(_T("投递 AcceptEx 请求失败，错误代码: %d"), WSAGetLastError());
			return false;  
		}  
	} 

	return true;
}
```

这里我开始准备了 **MAX_POST_ACCEPT = 10** 个 socket。



而工作线程的线程函数应该看起来是这个样子（伪代码）：

```cpp
DWORD ThreadFunction()
{
	//使用GetQueuedCompletionStatus函数检测事件类型	
	if (事件类型 == 有新客户端连成功)
	{
		//做一些操作1，比如显示一个新连接信息
	}
	else if (事件类型 == 收到了一份数据)
	{
		//做一些操作2，比如解析数据
	}
	else if (事件类型 == 数据发送成功了)
	{
		//做一些操作3，比如显示一条数据发送成功信息
	}
}
```

在没有事件发生时，函数 **GetQueuedCompletionStatus()** 会让工作线程挂起，不会占用 CPU 时间片。



但是不知道你有没有发现线程函数存在以下问题：

1. **GetQueuedCompletionStatus** 函数如何确定事件类型？如何判断哪些事件是客户端连接成功事件，哪些事件是收发数据成功事件呢？
2. 当一个完成端口句柄上绑定多个 **socket** 时，这些 socket 有的是侦听 socket，有的是客户端 socket，如何判断到底是哪个 socket 上的事件呢？

奥妙就在某个 socket 与完成端口句柄绑定时的第三个参数 **CompletionKey**，这其实就是一个指针。让我们再看一下将 socket 与完成端口句柄绑定起来的 API **CreateIoCompletionPort** 函数的签名（**注意函数第三个参数**）：

```cpp
HANDLE WINAPI CreateIoCompletionPort(
  _In_     HANDLE    FileHandle,
  _In_opt_ HANDLE    ExistingCompletionPort,
  _In_     ULONG_PTR CompletionKey,
  _In_     DWORD     NumberOfConcurrentThreads
);
```

而 **GetQueuedCompletionStatus** 函数签名如下：

```cpp
BOOL WINAPI GetQueuedCompletionStatus(
  _In_  HANDLE       CompletionPort,
  _Out_ LPDWORD      lpNumberOfBytes,
  _Out_ PULONG_PTR   lpCompletionKey,
  _Out_ LPOVERLAPPED *lpOverlapped,
  _In_  DWORD        dwMilliseconds
);
```

看到没有，**GetQueuedCompletionStatus** 正好也有一个参数叫 **CompletionPort**，而且还是一个输出参数。没错！这两个其实就是同一个指针。这样如果我在绑定 socket 到完成端口句柄时使用一块内存的指针作为**CompletionKey** 的值，该内存携带该 socket 的信息，这样我在工作线程中收到事件通知时就能取出这个CompletionKey 来得到这个 socket 句柄了，这样我就知道到底是哪个 socket 上的事件了。伪码如下：

```cpp
struct SOME_STRUCT
{
	SOCKET s;
	//可以再定义一些其它信息一起携带
};

//对于侦听socket
SOME_STRUCT someStruct1;
someStruct1.s = ListenSocket;
CreateIoCompletionPort( ListenSocket, 
                       m_hIOCompletionPort, 
                       (ULONG_PTR)&someStruct1,
                       0);

//对于普通客户端连接socket
SOME_STRUCT someStruct2;
someStruct2.s = acceptSocket;
CreateIoCompletionPort( acceptSocket, 
                       m_hIOCompletionPort,
                       (ULONG_PTR)&someStruct2,
                       0);
```

其实这个 **SOME_STRUCT** 因为是每一个 socket 有一份，所以它有个名字叫“Per Socket Data”（中文：**单 socket 数据**）。



线程函数里面就应该写成这个样子：

```cpp
DWORD ThreadFunction()
{
	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;
	
	BOOL bReturn = GetQueuedCompletionStatus(m_hIOCompletionPort, &dwBytesTransfered, (PULONG_PTR)&pSocketContext, &pOverlapped, INFINITE);
	
	if (((SOME_STRUCT*)pSocketContext)->s == 侦听socket句柄)
	{
		//新连接接收成功,做一些操作
	}
	//普通客户端socket收发数据
	else
	{
		if (事件类型 == 收到了一份数据)
		{
			//做一些操作2，比如解析数据
		}
		else if (事件类型 == 数据发送成功了)
		{
			//做一些操作3，比如显示一条数据发送成功信息
		}
	}
	
}
```

现在另外一个问题就是：**如何判断是数据发送成功还是收到了数据？**前面已经说过，对于每一次的收发数据，都需要调用 **WSASend** 或 **WSARecv** 函数进行准备，而这两个函数需要一个 **OVERLAPPED** 结构体，实际传的是这个结构体的指针。既然是指针类型，我们可以根据指针对象的伸缩特性，在这个 **OVERLAPPED** 结构体后面再增加一些字段来标识我们是收数据动作还是发数据动作。而这个扩展的 OVERLAPPED结 构体，因为是针对每一次 IO 操作的，所以叫“**Per IO Data**”（中文：**单 IO 数据**）。**因此这个数据结构的第一个字段必须是一个 OVERLAPPED  结构体**：

```cpp
typedef struct _PER_IO_CONTEXT
{
	// 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)              
    OVERLAPPED     m_Overlapped;
    // 这个网络操作所使用的Socket
	SOCKET         m_sockAccept;  
    // WSA类型的缓冲区，用于给重叠操作传参数的
	WSABUF         m_wsaBuf;  
    // 这个是WSABUF里具体存字符的缓冲区
	char           m_szBuffer[MAX_BUFFER_LEN]; 
    // 标识网络操作的类型(对应上面的枚举)
	OPERATION_TYPE m_OpType;                                   
};
```



我们首先将 **SOME_STRUCT** 改名成它应该叫的名字，即 **_PER_SOCKET_CONTEXT**：

```cpp
typedef struct _PER_SOCKET_CONTEXT
{  
	// 每一个客户端连接的Socket
    SOCKET      m_Socket;     
    // 客户端的地址
	SOCKADDR_IN m_ClientAddr;
    // 客户端网络操作的上下文数据，
	CArray<_PER_IO_CONTEXT*> m_arrayIoContext;             
};
```



我们再次观察 **GetQueuedCompletionStatus** 的函数签名会发现，其第三个参数正好就是一个 **OVERLAPPED** 结构指针，至此我们在工作线程里面不仅可以知道是哪个 socket 的事件，同时能通过 **OVERLAPPED*** 后面的字段知道是收数据（广义上说是读事件）还是发数据（广义上说是写事件）等事件类型：

```cpp
DWORD ThreadFunction()
{
	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;
	
	BOOL bReturn = GetQueuedCompletionStatus(m_hIOCompletionPort, &dwBytesTransfered, (PULONG_PTR)&pSocketContext, &pOverlapped, INFINITE);
	
	if (((SOME_STRUCT*)pSocketContext)->s == 侦听socket句柄)
	{
		//新连接接收成功,做一些操作
	}
	//普通客户端socket收发数据
	else
	{
		//通过pOverlapped结构得到pIOContext
		PER_IO_CONTEXT* pIOContext = (PER_IO_CONTEXT*)pOverlapped;	
		if (pIOContext->Type == 收)
		{
			//做一些操作2，比如解析数据
		}
		else if (pIOContext->Type == 发)
		{
			//做一些操作3，比如显示一条数据发送成功信息
		}
	}
	
}
```

上述代码中小结构体指针转换成大结构体指针操作：

```
PER_IO_CONTEXT* pIOContext = (PER_IO_CONTEXT*)pOverlapped;
```

当然，微软考虑到我们在使用完成端口模型时需要经常做这种指针伸缩性转换，直接帮我们定义了一个便利的工具宏 **CONTAINING_RECORD** 来方便我们进行操作：

```cpp
//
// Calculate the address of the base of the structure given its type, and an
// address of a field within the structure.
//
#define CONTAINING_RECORD(address, type, field) ((type *)( \
                                                  (PCHAR)(address) - \
                                                  (ULONG_PTR)(&((type *)0)->field)))
```

所以上述代码也可以写成：

```
PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, 
											   m_Overlapped);  
```

不知道读者是否记得前面中说过每消耗一个预先准备客户端的 socket，就要补上一个。这个代码现在看来就应该放在连接成功事件里面了：

```cpp
DWORD ThreadFunction()
{
	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;
	
	BOOL bReturn = GetQueuedCompletionStatus(m_hIOCompletionPort, &dwBytesTransfered, (PULONG_PTR)&pSocketContext, &pOverlapped, INFINITE);
	
	if (((SOME_STRUCT*)pSocketContext)->s == 侦听socket句柄)
	{
		//连接成功后可以做以下事情：
		//1. 获取对端和本端的ip地址和端口号,即AcceptEx的第三个参数lpOutputBuffer中拿(这一步，不是必须)
		//2. 如果对端连接成功后会发数据过来，则可以从初始化时调用AcceptEx准备的缓冲区里面拿到，即AcceptEx的第三个参数lpOutputBuffer中拿（这一步不是必须）
		
		//3. 再次调用AcceptEx补充一个sAcceptSocket（这一步是必须的）
	}
	//普通客户端socket收发数据
	else
	{
		//通过pOverlapped结构得到pIOContext
		PER_IO_CONTEXT* pIOContext = (PER_IO_CONTEXT*)pOverlapped;	
		if (pIOContext->Type == 收)
		{
			//做一些操作2，比如解析数据
		}
		else if (pIOContext->Type == 发)
		{
			//做一些操作3，比如显示一条数据发送成功信息
		}
	}// end if
}
```

上面连接成功后的伪码，第 1 步和第 2 步不是必须的，而第 3 步是必须的，如果不及时补充的话，等连接数多于准备的 socket，可能就无法接受新的客户端连接了。

因为两端的地址信息和对端发过来的第一组数据都在同一个缓冲区里面，让我们再看一下 **AcceptEx** 函数签名吧：

```cpp
BOOL AcceptEx(
  _In_  SOCKET       sListenSocket,
  _In_  SOCKET       sAcceptSocket,
  _In_  PVOID        lpOutputBuffer,
  _In_  DWORD        dwReceiveDataLength,
  _In_  DWORD        dwLocalAddressLength,
  _In_  DWORD        dwRemoteAddressLength,
  _Out_ LPDWORD      lpdwBytesReceived,
  _In_  LPOVERLAPPED lpOverlapped
);
```

虽然可以根据 **dwReceiveDataLength**、**dwLocalAddressLength**、**dwRemoteAddressLength**、**lpdwBytesReceived** 这几个参数计算出来，但是微软提供了一个函数来帮我们做这个解析动作——  **GetAcceptExSockaddrs** 。同理，这个函数最好也要通过 **WSAIoctl** 函数来动态获取：

```cpp
// 获取GetAcceptExSockAddrs函数指针，也是同理
if(SOCKET_ERROR == WSAIoctl(
	m_pListenContext->m_Socket, 
	SIO_GET_EXTENSION_FUNCTION_POINTER, 
	&GuidGetAcceptExSockAddrs,
	sizeof(GuidGetAcceptExSockAddrs), 
	&m_lpfnGetAcceptExSockAddrs, 
	sizeof(m_lpfnGetAcceptExSockAddrs),   
	&dwBytes, 
	NULL, 
	NULL))  
{  
	this->_ShowMessage(_T("WSAIoctl 未能获取GuidGetAcceptExSockAddrs函数指针。错误代码: %d\n"), WSAGetLastError());
	this->_DeInitialize();
	return false; 
}
```

然后使用返回的函数指针来使用函数 **GetAcceptExSockaddrs** （https://msdn.microsoft.com/en-us/library/windows/desktop/ms738516(v=vs.85).aspx）。解析地址信息和第一组数据的代码如下：

```cpp
SOCKADDR_IN* ClientAddr = NULL;
SOCKADDR_IN* LocalAddr = NULL; 
int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN); 
// 1. 首先取得连入客户端的地址信息 
// 这个m_lpfnGetAcceptExSockAddrs
// 不但可以取得客户端和本地端的地址信息，还能顺便取出客户端发来的第一组数据。 
this->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, 
                                 pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16)*2),
                                 sizeof(SOCKADDR_IN) + 16,
                                 sizeof(SOCKADDR_IN) + 16, 
                                 (LPSOCKADDR*)&LocalAddr, 
                                 &localLen, 
                                 (LPSOCKADDR*)&ClientAddr, &remoteLen);
this->_ShowMessage( _T("客户端 %s:%d 连入."),
                   inet_ntoa(ClientAddr->sin_addr), 
                   ntohs(ClientAddr->sin_port) ); 
this->_ShowMessage( _T("客户额 %s:%d 信息：%s."), 
                   inet_ntoa(ClientAddr->sin_addr), 
                   ntohs(ClientAddr->sin_port),
                   pIoContext->m_wsaBuf.buf ); 
```

以上介绍的是接收新连接成功后的处理，那收数据和发数据的准备工作在哪里做呢？（收取第一组数据可以在调用 **AcceptEx** 的地方做）。这个就仁者见仁，智者见智了。你可以在新连接接收成功之后，立即准备给对端发数据；或者在收到对端数据的时候准备给对端发数据；再或者在发送数据完成后准备收对端数据。伪码如下：

```cpp
DWORD ThreadFunction()
{
	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;
	
	BOOL bReturn = GetQueuedCompletionStatus(m_hIOCompletionPort, &dwBytesTransfered, (PULONG_PTR)&pSocketContext, &pOverlapped, INFINITE);
	
	if (((SOME_STRUCT*)pSocketContext)->s == 侦听socket句柄)
	{
		//连接成功后可以做以下事情：
		//1. 获取对端和本端的ip地址和端口号,即AcceptEx的第三个参数lpOutputBuffer中拿(这一步，不是必须)
		//2. 如果对端连接成功后会发数据过来，则可以从初始化时调用AcceptEx准备的缓冲区里面拿到，即AcceptEx的第三个参数lpOutputBuffer中拿（这一步不是必须）
		
		//3. 再次调用AcceptEx补充一个sAcceptSocket（这一步是必须的）
		
		//4. 调用WSASend准备发送数据工作或调用WSARecv准备接收数据工作(这一步，不是必须)
	}
	//普通客户端socket收发数据
	else
	{
		//通过pOverlapped结构得到pIOContext
		PER_IO_CONTEXT* pIOContext = (PER_IO_CONTEXT*)pOverlapped;	
		if (pIOContext->Type == 收)
		{			
			//解析收到的数据(这一步，不是必须)
			//调用WSASend准备发送数据工作（比如应答客户端）(这一步，不是必须)
			//继续调用WSARecv准备收取数据工作(这一步，不是必须)
		}
		else if (pIOContext->Type == 发)
		{
			//调用WSARecv准备收取数据工作(这一步，不是必须)
		}
	}// end if
}
```



现在还剩下最后一个问题，就是**工作线程如何退出**？

当然你可以在工作线程函数里面设置一个退出标志，通过判断这个标识位来确定是否结束工作线程函数。但是这个方法不能解决工作线程正好被 **GetQueuedCompletionStatus** 挂起那里的问题。我们需要先唤醒被挂起的工作线程，如何唤醒？微软提供了另外一个函数：**PostQueuedCompletionStatus**，看下这个函数的签名：

```cpp
BOOL WINAPI PostQueuedCompletionStatus(
  _In_     HANDLE       CompletionPort,
  _In_     DWORD        dwNumberOfBytesTransferred,
  _In_     ULONG_PTR    dwCompletionKey,
  _In_opt_ LPOVERLAPPED lpOverlapped
);
```

这个函数可以唤醒被 **GetQueuedCompletionStatus** 函数挂起的工作线程，当然其第三个参数也是一个 **CompletionKey**（即参数 **dwCompletionKey**）。你可以使用这个 **dwCompletionKey** 做标识干一些其它的事情，当然设置一个退出码也可以。例如：

```cpp
PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
```

这样工作线程里面就可以使用 **EXIT_CODE** 来作为退出标志：

```cpp
DWORD ThreadFunction()
{
	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;
	
	BOOL bReturn = GetQueuedCompletionStatus(m_hIOCompletionPort, 
                                             &dwBytesTransfered, 
                                             (PULONG_PTR)&pSocketContext, 
                                             &pOverlapped, 
                                             INFINITE);
	
	// 如果收到的是退出标志，则直接退出
	if ( EXIT_CODE==(DWORD)pSocketContext )
	{
		return 0;
	}
	
	if (((SOME_STRUCT*)pSocketContext)->s == 侦听socket句柄)
	{
		//连接成功后可以做以下事情：
		//1. 获取对端和本端的ip地址和端口号,即AcceptEx的第三个参数lpOutputBuffer中拿(这一步，不是必须)
		//2. 如果对端连接成功后会发数据过来，则可以从初始化时调用AcceptEx准备的缓冲区里面拿到，即AcceptEx的第三个参数lpOutputBuffer中拿（这一步不是必须）
		
		//3. 再次调用AcceptEx补充一个sAcceptSocket（这一步是必须的）
		
		//4. 调用WSASend准备发送数据工作或调用WSARecv准备接收数据工作(这一步，不是必须)
	}
	//普通客户端socket收发数据
	else
	{
		//通过pOverlapped结构得到pIOContext
		PER_IO_CONTEXT* pIOContext = (PER_IO_CONTEXT*)pOverlapped;	
		if (pIOContext->Type == 收)
		{			
			//解析收到的数据(这一步，不是必须)
			//调用WSASend准备发送数据工作（比如应答客户端）(这一步，不是必须)
			//继续调用WSARecv准备收取数据工作(这一步，不是必须)
		}
		else if (pIOContext->Type == 发)
		{
			//调用WSARecv准备收取数据工作(这一步，不是必须)
		}
	}
	
	return 0;
}
```



至此，关于完成端口的东西就全部介绍完了。

#### 完成端口使用关键点总结

我们总结一下，掌握完成端口的关键在于理解以下几点：

1. 完成端口绑定了某个 socket 后，不仅其事件的读写检测由操作系统完成，而且就算是接受新连接、收发数据的动作也是由操作系统代劳了，操作系统完成后会通知你。等你收到通知时，一切都完成好了。你可以直接取出对应的数据使用。
2. 要想第 1 点介绍的事情由操作系统代劳，你必须预先准备很多数据结构，比如两端的地址结构体、收发缓冲区、用来表示新连接的 socket 等等，这些准备工作可能在程序初始化阶段，也可能在工作线程某个事件处理的地方。
3. 初始化准备好的各种缓冲区如何在工作线程里面引用到的关键就在于绑定完成端口时 **CompletionKey** 参数 和准备收发缓冲区时 **OVERLAPPED** 结构体的使用， **CompletionKey** 参数对应 **PER Socket Data**，**OVERLAPPED** 结构体对应 **Per IO Data**，即 **CompletionKey** 是**单 Socket 数据**，**OVERLAPPED** 结构体是**单 IO 数据**。



#### 完成端口完整代码示例

最后我们给出上文中使用到的对完成端口模型封装的类的全部代码：

**IOCPModel.h** 内容：

```cpp
/*
==========================================================================

Purpose:

	* 这个类CIOCPModel是本代码的核心类，用于说明WinSock服务器端编程模型中的
	  完成端口(IOCP)的使用方法，并使用MFC对话框程序来调用这个类实现了基本的
	  服务器网络通信的功能。

	* 其中的PER_IO_DATA结构体是封装了用于每一个重叠操作的参数
	  PER_HANDLE_DATA 是封装了用于每一个Socket的参数，也就是用于每一个完成端口的参数

	* 详细的文档说明请参考 http://blog.csdn.net/PiggyXP

Notes:

	* 具体讲明了服务器端建立完成端口、建立工作者线程、投递Recv请求、投递Accept请求的方法，
	  所有的客户端连入的Socket都需要绑定到IOCP上，所有从客户端发来的数据，都会实时显示到
	  主界面中去。

Author:

	* PiggyXP【小猪】

Date:

	* 2009/10/04

==========================================================================
*/

#pragma once

// winsock 2 的头文件和库
#include <winsock2.h>
#include <MSWSock.h>
#pragma comment(lib,"ws2_32.lib")

// 缓冲区长度 (1024*8)
// 之所以为什么设置8K，也是一个江湖上的经验值
// 如果确实客户端发来的每组数据都比较少，那么就设置得小一些，省内存
#define MAX_BUFFER_LEN        8192  
// 默认端口
#define DEFAULT_PORT          12345    
// 默认IP地址
#define DEFAULT_IP            _T("127.0.0.1")


//////////////////////////////////////////////////////////////////
// 在完成端口上投递的I/O操作的类型
typedef enum _OPERATION_TYPE  
{  
	ACCEPT_POSTED,                     // 标志投递的Accept操作
	SEND_POSTED,                       // 标志投递的是发送操作
	RECV_POSTED,                       // 标志投递的是接收操作
	NULL_POSTED                        // 用于初始化，无意义

}OPERATION_TYPE;

//====================================================================================
//
//				单IO数据结构体定义(用于每一个重叠操作的参数)
//
//====================================================================================

typedef struct _PER_IO_CONTEXT
{
	OVERLAPPED     m_Overlapped;                               // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)              
	SOCKET         m_sockAccept;                               // 这个网络操作所使用的Socket
	WSABUF         m_wsaBuf;                                   // WSA类型的缓冲区，用于给重叠操作传参数的
	char           m_szBuffer[MAX_BUFFER_LEN];                 // 这个是WSABUF里具体存字符的缓冲区
	OPERATION_TYPE m_OpType;                                   // 标识网络操作的类型(对应上面的枚举)

	// 初始化
	_PER_IO_CONTEXT()
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));  
		ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
		m_sockAccept = INVALID_SOCKET;
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
		m_OpType     = NULL_POSTED;
	}
	// 释放掉Socket
	~_PER_IO_CONTEXT()
	{
		if( m_sockAccept!=INVALID_SOCKET )
		{
			closesocket(m_sockAccept);
			m_sockAccept = INVALID_SOCKET;
		}
	}
	// 重置缓冲区内容
	void ResetBuffer()
	{
		ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
	}

} PER_IO_CONTEXT, *PPER_IO_CONTEXT;


//====================================================================================
//
//				单句柄数据结构体定义(用于每一个完成端口，也就是每一个Socket的参数)
//
//====================================================================================

typedef struct _PER_SOCKET_CONTEXT
{  
	SOCKET      m_Socket;                                  // 每一个客户端连接的Socket
	SOCKADDR_IN m_ClientAddr;                              // 客户端的地址
	CArray<_PER_IO_CONTEXT*> m_arrayIoContext;             // 客户端网络操作的上下文数据，
	                                                       // 也就是说对于每一个客户端Socket，是可以在上面同时投递多个IO请求的

	// 初始化
	_PER_SOCKET_CONTEXT()
	{
		m_Socket = INVALID_SOCKET;
		memset(&m_ClientAddr, 0, sizeof(m_ClientAddr)); 
	}

	// 释放资源
	~_PER_SOCKET_CONTEXT()
	{
		if( m_Socket!=INVALID_SOCKET )
		{
			closesocket( m_Socket );
		    m_Socket = INVALID_SOCKET;
		}
		// 释放掉所有的IO上下文数据
		for( int i=0;i<m_arrayIoContext.GetCount();i++ )
		{
			delete m_arrayIoContext.GetAt(i);
		}
		m_arrayIoContext.RemoveAll();
	}

	// 获取一个新的IoContext
	_PER_IO_CONTEXT* GetNewIoContext()
	{
		_PER_IO_CONTEXT* p = new _PER_IO_CONTEXT;

		m_arrayIoContext.Add( p );

		return p;
	}

	// 从数组中移除一个指定的IoContext
	void RemoveContext( _PER_IO_CONTEXT* pContext )
	{
		ASSERT( pContext!=NULL );

		for( int i=0;i<m_arrayIoContext.GetCount();i++ )
		{
			if( pContext==m_arrayIoContext.GetAt(i) )
			{
				delete pContext;
				pContext = NULL;
				m_arrayIoContext.RemoveAt(i);				
				break;
			}
		}
	}

} PER_SOCKET_CONTEXT, *PPER_SOCKET_CONTEXT;




//====================================================================================
//
//				CIOCPModel类定义
//
//====================================================================================

// 工作者线程的线程参数
class CIOCPModel;
typedef struct _tagThreadParams_WORKER
{
	CIOCPModel* pIOCPModel;                                   // 类指针，用于调用类中的函数
	int         nThreadNo;                                    // 线程编号

} THREADPARAMS_WORKER,*PTHREADPARAM_WORKER; 

// CIOCPModel类
class CIOCPModel
{
public:
	CIOCPModel(void);
	~CIOCPModel(void);

public:

	// 启动服务器
	bool Start();

	//	停止服务器
	void Stop();

	// 加载Socket库
	bool LoadSocketLib();

	// 卸载Socket库，彻底完事
	void UnloadSocketLib() { WSACleanup(); }

	// 获得本机的IP地址
	CString GetLocalIP();

	// 设置监听端口
	void SetPort( const int& nPort ) { m_nPort=nPort; }

	// 设置主界面的指针，用于调用显示信息到界面中
	void SetMainDlg( CDialog* p ) { m_pMain=p; }

protected:

	// 初始化IOCP
	bool _InitializeIOCP();

	// 初始化Socket
	bool _InitializeListenSocket();

	// 最后释放资源
	void _DeInitialize();

	// 投递Accept请求
	bool _PostAccept( PER_IO_CONTEXT* pAcceptIoContext ); 

	// 投递接收数据请求
	bool _PostRecv( PER_IO_CONTEXT* pIoContext );

	// 在有客户端连入的时候，进行处理
	bool _DoAccpet( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext );

	// 在有接收的数据到达的时候，进行处理
	bool _DoRecv( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext );

	// 将客户端的相关信息存储到数组中
	void _AddToContextList( PER_SOCKET_CONTEXT *pSocketContext );

	// 将客户端的信息从数组中移除
	void _RemoveContext( PER_SOCKET_CONTEXT *pSocketContext );

	// 清空客户端信息
	void _ClearContextList();

	// 将句柄绑定到完成端口中
	bool _AssociateWithIOCP( PER_SOCKET_CONTEXT *pContext);

	// 处理完成端口上的错误
	bool HandleError( PER_SOCKET_CONTEXT *pContext,const DWORD& dwErr );

	// 线程函数，为IOCP请求服务的工作者线程
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	// 获得本机的处理器数量
	int _GetNoOfProcessors();

	// 判断客户端Socket是否已经断开
	bool _IsSocketAlive(SOCKET s);

	// 在主界面中显示信息
	void _ShowMessage( const CString szFormat,...) const;

private:

	HANDLE                       m_hShutdownEvent;              // 用来通知线程系统退出的事件，为了能够更好的退出线程

	HANDLE                       m_hIOCompletionPort;           // 完成端口的句柄

	HANDLE*                      m_phWorkerThreads;             // 工作者线程的句柄指针

	int		                     m_nThreads;                    // 生成的线程数量

	CString                      m_strIP;                       // 服务器端的IP地址
	int                          m_nPort;                       // 服务器端的监听端口

	CDialog*                     m_pMain;                       // 主界面的界面指针，用于在主界面中显示消息

	CRITICAL_SECTION             m_csContextList;               // 用于Worker线程同步的互斥量

	CArray<PER_SOCKET_CONTEXT*>  m_arrayClientContext;          // 客户端Socket的Context信息        

	PER_SOCKET_CONTEXT*          m_pListenContext;              // 用于监听的Socket的Context信息

	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
	LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs; 

};
```



**IOCPModel.cpp** 内容：

```cpp
#include "StdAfx.h"
#include "IOCPModel.h"
#include "MainDlg.h"

// 每一个处理器上产生多少个线程(为了最大限度的提升服务器性能，详见配套文档)
#define WORKER_THREADS_PER_PROCESSOR 2
// 同时投递的Accept请求的数量(这个要根据实际的情况灵活设置)
#define MAX_POST_ACCEPT              10
// 传递给Worker线程的退出信号
#define EXIT_CODE                    NULL


// 释放指针和句柄资源的宏

// 释放指针宏
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// 释放句柄宏
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// 释放Socket宏
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}



CIOCPModel::CIOCPModel(void):
							m_nThreads(0),
							m_hShutdownEvent(NULL),
							m_hIOCompletionPort(NULL),
							m_phWorkerThreads(NULL),
							m_strIP(DEFAULT_IP),
							m_nPort(DEFAULT_PORT),
							m_pMain(NULL),
							m_lpfnAcceptEx( NULL ),
							m_pListenContext( NULL )
{
}


CIOCPModel::~CIOCPModel(void)
{
	// 确保资源彻底释放
	this->Stop();
}




///////////////////////////////////////////////////////////////////
// 工作者线程：  为IOCP请求服务的工作者线程
//         也就是每当完成端口上出现了完成数据包，就将之取出来进行处理的线程
///////////////////////////////////////////////////////////////////

DWORD WINAPI CIOCPModel::_WorkerThread(LPVOID lpParam)
{    
	THREADPARAMS_WORKER* pParam = (THREADPARAMS_WORKER*)lpParam;
	CIOCPModel* pIOCPModel = (CIOCPModel*)pParam->pIOCPModel;
	int nThreadNo = (int)pParam->nThreadNo;

	pIOCPModel->_ShowMessage(_T("工作者线程启动，ID: %d."),nThreadNo);

	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;

	// 循环处理请求，知道接收到Shutdown信息为止
	while (WAIT_OBJECT_0 != WaitForSingleObject(pIOCPModel->m_hShutdownEvent, 0))
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			pIOCPModel->m_hIOCompletionPort,
			&dwBytesTransfered,
			(PULONG_PTR)&pSocketContext,
			&pOverlapped,
			INFINITE);

		// 如果收到的是退出标志，则直接退出
		if ( EXIT_CODE==(DWORD)pSocketContext )
		{
			break;
		}

		// 判断是否出现了错误
		if( !bReturn )  
		{  
			DWORD dwErr = GetLastError();

			// 显示一下提示信息
			if( !pIOCPModel->HandleError( pSocketContext,dwErr ) )
			{
				break;
			}

			continue;  
		}  
		else  
		{  	
			// 读取传入的参数
			PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  

			// 判断是否有客户端断开了
			if((0 == dwBytesTransfered) && ( RECV_POSTED==pIoContext->m_OpType || SEND_POSTED==pIoContext->m_OpType))  
			{  
				pIOCPModel->_ShowMessage( _T("客户端 %s:%d 断开连接."),inet_ntoa(pSocketContext->m_ClientAddr.sin_addr), ntohs(pSocketContext->m_ClientAddr.sin_port) );

				// 释放掉对应的资源
				pIOCPModel->_RemoveContext( pSocketContext );

 				continue;  
			}  
			else
			{
				switch( pIoContext->m_OpType )  
				{  
					 // Accept  
				case ACCEPT_POSTED:
					{ 

						// 为了增加代码可读性，这里用专门的_DoAccept函数进行处理连入请求
						pIOCPModel->_DoAccpet( pSocketContext, pIoContext );						
						

					}
					break;

					// RECV
				case RECV_POSTED:
					{
						// 为了增加代码可读性，这里用专门的_DoRecv函数进行处理接收请求
						pIOCPModel->_DoRecv( pSocketContext,pIoContext );
					}
					break;

					// SEND
					// 这里略过不写了，要不代码太多了，不容易理解，Send操作相对来讲简单一些
				case SEND_POSTED:
					{

					}
					break;
				default:
					// 不应该执行到这里
					TRACE(_T("_WorkThread中的 pIoContext->m_OpType 参数异常.\n"));
					break;
				} //switch
			}//if
		}//if

	}//while

	TRACE(_T("工作者线程 %d 号退出.\n"),nThreadNo);

	// 释放线程参数
	RELEASE(lpParam);	

	return 0;
}



//====================================================================================
//
//				    系统初始化和终止
//
//====================================================================================




////////////////////////////////////////////////////////////////////
// 初始化WinSock 2.2
bool CIOCPModel::LoadSocketLib()
{    
	WSADATA wsaData;
	int nResult;
	nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	// 错误(一般都不可能出现)
	if (NO_ERROR != nResult)
	{
		this->_ShowMessage(_T("初始化WinSock 2.2失败！\n"));
		return false; 
	}

	return true;
}

//////////////////////////////////////////////////////////////////
//	启动服务器
bool CIOCPModel::Start()
{
	// 初始化线程互斥量
	InitializeCriticalSection(&m_csContextList);

	// 建立系统退出的事件通知
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// 初始化IOCP
	if (false == _InitializeIOCP())
	{
		this->_ShowMessage(_T("初始化IOCP失败！\n"));
		return false;
	}
	else
	{
        this->_ShowMessage(_T("\nIOCP初始化完毕\n."));
	}

	// 初始化Socket
	if( false==_InitializeListenSocket() )
	{
		this->_ShowMessage(_T("Listen Socket初始化失败！\n"));
		this->_DeInitialize();
		return false;
	}
	else
	{
        this->_ShowMessage(_T("Listen Socket初始化完毕."));
	}

	this->_ShowMessage(_T("系统准备就绪，等候连接....\n"));

	return true;
}


////////////////////////////////////////////////////////////////////
//	开始发送系统退出消息，退出完成端口和线程资源
void CIOCPModel::Stop()
{
	if( m_pListenContext!=NULL && m_pListenContext->m_Socket!=INVALID_SOCKET )
	{
		// 激活关闭消息通知
		SetEvent(m_hShutdownEvent);

		for (int i = 0; i < m_nThreads; i++)
		{
			// 通知所有的完成端口操作退出
			PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
		}

		// 等待所有的客户端资源退出
		WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);

		// 清除客户端列表信息
		this->_ClearContextList();

		// 释放其他资源
		this->_DeInitialize();

        this->_ShowMessage(_T("停止监听\n"));
	}	
}


////////////////////////////////
// 初始化完成端口
bool CIOCPModel::_InitializeIOCP()
{
	// 建立第一个完成端口
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );

	if ( NULL == m_hIOCompletionPort)
	{
		this->_ShowMessage(_T("建立完成端口失败！错误代码: %d!\n"), WSAGetLastError());
		return false;
	}

	// 根据本机中的处理器数量，建立对应的线程数
	m_nThreads = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();
	
	// 为工作者线程初始化句柄
	m_phWorkerThreads = new HANDLE[m_nThreads];
	
	// 根据计算出来的数量建立工作者线程
	DWORD nThreadID;
	for (int i = 0; i < m_nThreads; i++)
	{
		THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
		pThreadParams->pIOCPModel = this;
		pThreadParams->nThreadNo  = i+1;
		m_phWorkerThreads[i] = ::CreateThread(0, 0, _WorkerThread, (void *)pThreadParams, 0, &nThreadID);
	}

	TRACE(" 建立 _WorkerThread %d 个.\n", m_nThreads );

	return true;
}


/////////////////////////////////////////////////////////////////
// 初始化Socket
bool CIOCPModel::_InitializeListenSocket()
{
	// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
	GUID GuidAcceptEx = WSAID_ACCEPTEX;  
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS; 

	// 服务器地址信息，用于绑定Socket
	struct sockaddr_in ServerAddress;

	// 生成用于监听的Socket的信息
	m_pListenContext = new PER_SOCKET_CONTEXT;

	// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
	m_pListenContext->m_Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == m_pListenContext->m_Socket) 
	{
        this->_ShowMessage(_T("初始化Socket失败，错误代码: %d.\n"), WSAGetLastError());
		return false;
	}
	else
	{
        TRACE(_T("WSASocket() 完成.\n"));
	}

	// 将Listen Socket绑定至完成端口中
	if( NULL== CreateIoCompletionPort( (HANDLE)m_pListenContext->m_Socket, m_hIOCompletionPort,(DWORD)m_pListenContext, 0))  
	{  
        this->_ShowMessage(_T("绑定 Listen Socket至完成端口失败！错误代码: %d/n"), WSAGetLastError());
		RELEASE_SOCKET( m_pListenContext->m_Socket );
		return false;
	}
	else
	{
        TRACE(_T("Listen Socket绑定完成端口 完成.\n"));
	}

	// 填充地址信息
	ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
	ServerAddress.sin_family = AF_INET;
	// 这里可以绑定任何可用的IP地址，或者绑定一个指定的IP地址 
	ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);                      
	//ServerAddress.sin_addr.s_addr = inet_addr(CStringA(m_strIP).GetString());         
	ServerAddress.sin_port = htons(m_nPort);                          

	// 绑定地址和端口
	if (SOCKET_ERROR == bind(m_pListenContext->m_Socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress))) 
	{
        this->_ShowMessage(_T("bind()函数执行错误.\n"));
		return false;
	}
	else
	{
        TRACE(_T("bind() 完成.\n"));
	}

	// 开始进行监听
	if (SOCKET_ERROR == listen(m_pListenContext->m_Socket,SOMAXCONN))
	{
        this->_ShowMessage(_T("Listen()函数执行出现错误.\n"));
		return false;
	}
	else
	{
        TRACE(_T("Listen() 完成.\n"));
	}

	// 使用AcceptEx函数，因为这个是属于WinSock2规范之外的微软另外提供的扩展函数
	// 所以需要额外获取一下函数的指针，
	// 获取AcceptEx函数指针
	DWORD dwBytes = 0;  
	if(SOCKET_ERROR == WSAIoctl(
		m_pListenContext->m_Socket, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidAcceptEx, 
		sizeof(GuidAcceptEx), 
		&m_lpfnAcceptEx, 
		sizeof(m_lpfnAcceptEx), 
		&dwBytes, 
		NULL, 
		NULL))  
	{  
        this->_ShowMessage(_T("WSAIoctl 未能获取AcceptEx函数指针。错误代码: %d\n"), WSAGetLastError());
		this->_DeInitialize();
		return false;  
	}  

	// 获取GetAcceptExSockAddrs函数指针，也是同理
	if(SOCKET_ERROR == WSAIoctl(
		m_pListenContext->m_Socket, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidGetAcceptExSockAddrs,
		sizeof(GuidGetAcceptExSockAddrs), 
		&m_lpfnGetAcceptExSockAddrs, 
		sizeof(m_lpfnGetAcceptExSockAddrs),   
		&dwBytes, 
		NULL, 
		NULL))  
	{  
        this->_ShowMessage(_T("WSAIoctl 未能获取GuidGetAcceptExSockAddrs函数指针。错误代码: %d\n"), WSAGetLastError());
		this->_DeInitialize();
		return false; 
	}  


	// 为AcceptEx 准备参数，然后投递AcceptEx I/O请求
	for( int i=0;i<MAX_POST_ACCEPT;i++ )
	{
		// 新建一个IO_CONTEXT
		PER_IO_CONTEXT* pAcceptIoContext = m_pListenContext->GetNewIoContext();

		if( false==this->_PostAccept( pAcceptIoContext ) )
		{
			m_pListenContext->RemoveContext(pAcceptIoContext);
			return false;
		}
	}

	this->_ShowMessage( _T("投递 %d 个AcceptEx请求完毕"),MAX_POST_ACCEPT );

	return true;
}

////////////////////////////////////////////////////////////
//	最后释放掉所有资源
void CIOCPModel::_DeInitialize()
{
	// 删除客户端列表的互斥量
	DeleteCriticalSection(&m_csContextList);

	// 关闭系统退出事件句柄
	RELEASE_HANDLE(m_hShutdownEvent);

	// 释放工作者线程句柄指针
	for( int i=0;i<m_nThreads;i++ )
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}
	
	RELEASE(m_phWorkerThreads);

	// 关闭IOCP句柄
	RELEASE_HANDLE(m_hIOCompletionPort);

	// 关闭监听Socket
	RELEASE(m_pListenContext);

    this->_ShowMessage(_T("释放资源完毕.\n"));
}


//====================================================================================
//
//				    投递完成端口请求
//
//====================================================================================


//////////////////////////////////////////////////////////////////
// 投递Accept请求
bool CIOCPModel::_PostAccept( PER_IO_CONTEXT* pAcceptIoContext )
{
	ASSERT( INVALID_SOCKET!=m_pListenContext->m_Socket );

	// 准备参数
	DWORD dwBytes = 0;  
	pAcceptIoContext->m_OpType = ACCEPT_POSTED;  
	WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;
	
	// 为以后新连入的客户端先准备好Socket( 这个是与传统accept最大的区别 ) 
	pAcceptIoContext->m_sockAccept  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  
	if( INVALID_SOCKET==pAcceptIoContext->m_sockAccept )  
	{  
        _ShowMessage(_T("创建用于Accept的Socket失败！错误代码: %d"), WSAGetLastError());
		return false;  
	} 

	// 投递AcceptEx
	if(FALSE == m_lpfnAcceptEx( m_pListenContext->m_Socket, pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),   
								sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))  
	{  
		if(WSA_IO_PENDING != WSAGetLastError())  
		{  
            _ShowMessage(_T("投递 AcceptEx 请求失败，错误代码: %d"), WSAGetLastError());
			return false;  
		}  
	} 

	return true;
}

////////////////////////////////////////////////////////////
// 在有客户端连入的时候，进行处理
// 流程有点复杂，你要是看不懂的话，就看配套的文档吧....
// 如果能理解这里的话，完成端口的机制你就消化了一大半了

// 总之你要知道，传入的是ListenSocket的Context，我们需要复制一份出来给新连入的Socket用
// 原来的Context还是要在上面继续投递下一个Accept请求
//
bool CIOCPModel::_DoAccpet( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext )
{
	SOCKADDR_IN* ClientAddr = NULL;
	SOCKADDR_IN* LocalAddr = NULL;  
	int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);  

	///////////////////////////////////////////////////////////////////////////
	// 1. 首先取得连入客户端的地址信息
	// 这个 m_lpfnGetAcceptExSockAddrs 不得了啊~~~~~~
	// 不但可以取得客户端和本地端的地址信息，还能顺便取出客户端发来的第一组数据，老强大了...
	this->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16)*2),  
		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);  

	this->_ShowMessage( _T("客户端 %s:%d 连入."), inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port) );
	this->_ShowMessage( _T("客户额 %s:%d 信息：%s."),inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pIoContext->m_wsaBuf.buf );


	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 这里需要注意，这里传入的这个是ListenSocket上的Context，这个Context我们还需要用于监听下一个连接
	// 所以我还得要将ListenSocket上的Context复制出来一份为新连入的Socket新建一个SocketContext

	PER_SOCKET_CONTEXT* pNewSocketContext = new PER_SOCKET_CONTEXT;
	pNewSocketContext->m_Socket           = pIoContext->m_sockAccept;
	memcpy(&(pNewSocketContext->m_ClientAddr), ClientAddr, sizeof(SOCKADDR_IN));

	// 参数设置完毕，将这个Socket和完成端口绑定(这也是一个关键步骤)
	if( false==this->_AssociateWithIOCP( pNewSocketContext ) )
	{
		RELEASE( pNewSocketContext );
		return false;
	}  


	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. 继续，建立其下的IoContext，用于在这个Socket上投递第一个Recv数据请求
	PER_IO_CONTEXT* pNewIoContext = pNewSocketContext->GetNewIoContext();
	pNewIoContext->m_OpType       = RECV_POSTED;
	pNewIoContext->m_sockAccept   = pNewSocketContext->m_Socket;
	// 如果Buffer需要保留，就自己拷贝一份出来
	//memcpy( pNewIoContext->m_szBuffer,pIoContext->m_szBuffer,MAX_BUFFER_LEN );

	// 绑定完毕之后，就可以开始在这个Socket上投递完成请求了
	if( false==this->_PostRecv( pNewIoContext) )
	{
		pNewSocketContext->RemoveContext( pNewIoContext );
		return false;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. 如果投递成功，那么就把这个有效的客户端信息，加入到ContextList中去(需要统一管理，方便释放资源)
	this->_AddToContextList( pNewSocketContext );

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. 使用完毕之后，把Listen Socket的那个IoContext重置，然后准备投递新的AcceptEx
	pIoContext->ResetBuffer();
	return this->_PostAccept( pIoContext ); 	
}

////////////////////////////////////////////////////////////////////
// 投递接收数据请求
bool CIOCPModel::_PostRecv( PER_IO_CONTEXT* pIoContext )
{
	// 初始化变量
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	pIoContext->ResetBuffer();
	pIoContext->m_OpType = RECV_POSTED;

	// 初始化完成后，，投递WSARecv请求
	int nBytesRecv = WSARecv( pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		this->_ShowMessage(_T("投递第一个WSARecv失败！"));
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////
// 在有接收的数据到达的时候，进行处理
bool CIOCPModel::_DoRecv( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext )
{
	// 先把上一次的数据显示出现，然后就重置状态，发出下一个Recv请求
	SOCKADDR_IN* ClientAddr = &pSocketContext->m_ClientAddr;
	this->_ShowMessage( _T("收到  %s:%d 信息：%s"),inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pIoContext->m_wsaBuf.buf );

	// 然后开始投递下一个WSARecv请求
	return _PostRecv( pIoContext );
}



/////////////////////////////////////////////////////
// 将句柄(Socket)绑定到完成端口中
bool CIOCPModel::_AssociateWithIOCP( PER_SOCKET_CONTEXT *pContext )
{
	// 将用于和客户端通信的SOCKET绑定到完成端口中
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->m_Socket, m_hIOCompletionPort, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		this->_ShowMessage(_T("执行CreateIoCompletionPort()出现错误.错误代码：%d"),GetLastError());
		return false;
	}

	return true;
}




//====================================================================================
//
//				    ContextList 相关操作
//
//====================================================================================


//////////////////////////////////////////////////////////////
// 将客户端的相关信息存储到数组中
void CIOCPModel::_AddToContextList( PER_SOCKET_CONTEXT *pHandleData )
{
	EnterCriticalSection(&m_csContextList);

	m_arrayClientContext.Add(pHandleData);	
	
	LeaveCriticalSection(&m_csContextList);
}

////////////////////////////////////////////////////////////////
//	移除某个特定的Context
void CIOCPModel::_RemoveContext( PER_SOCKET_CONTEXT *pSocketContext )
{
	EnterCriticalSection(&m_csContextList);

	for( int i=0;i<m_arrayClientContext.GetCount();i++ )
	{
		if( pSocketContext==m_arrayClientContext.GetAt(i) )
		{
			RELEASE( pSocketContext );			
			m_arrayClientContext.RemoveAt(i);			
			break;
		}
	}

	LeaveCriticalSection(&m_csContextList);
}

////////////////////////////////////////////////////////////////
// 清空客户端信息
void CIOCPModel::_ClearContextList()
{
	EnterCriticalSection(&m_csContextList);

	for( int i=0;i<m_arrayClientContext.GetCount();i++ )
	{
		delete m_arrayClientContext.GetAt(i);
	}

	m_arrayClientContext.RemoveAll();

	LeaveCriticalSection(&m_csContextList);
}



//====================================================================================
//
//				       其他辅助函数定义
//
//====================================================================================



////////////////////////////////////////////////////////////////////
// 获得本机的IP地址
CString CIOCPModel::GetLocalIP()
{
	// 获得本机主机名
	char hostname[MAX_PATH] = {0};
	gethostname(hostname,MAX_PATH);                
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if(lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}

	// 取得IP地址列表中的第一个为返回的IP(因为一台主机可能会绑定多个IP)
	LPSTR lpAddr = lpHostEnt->h_addr_list[0];      

	// 将IP地址转化成字符串形式
	struct in_addr inAddr;
	memmove(&inAddr,lpAddr,4);
	m_strIP = CString( inet_ntoa(inAddr) );        

	return m_strIP;
}

///////////////////////////////////////////////////////////////////
// 获得本机中处理器的数量
int CIOCPModel::_GetNoOfProcessors()
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);

	return si.dwNumberOfProcessors;
}

/////////////////////////////////////////////////////////////////////
// 在主界面中显示提示信息
void CIOCPModel::_ShowMessage(const CString szFormat,...) const
{
	// 根据传入的参数格式化字符串
	CString   strMessage;
	va_list   arglist;

	// 处理变长参数
	va_start(arglist, szFormat);
	strMessage.FormatV(szFormat,arglist);
	va_end(arglist);

	// 在主界面中显示
	CMainDlg* pMain = (CMainDlg*)m_pMain;
	if( m_pMain!=NULL )
	{
		pMain->AddInformation(strMessage);
		TRACE( strMessage+_T("\n") );
	}	
}

/////////////////////////////////////////////////////////////////////
// 判断客户端Socket是否已经断开，否则在一个无效的Socket上投递WSARecv操作会出现异常
// 使用的方法是尝试向这个socket发送数据，判断这个socket调用的返回值
// 因为如果客户端网络异常断开(例如客户端崩溃或者拔掉网线等)的时候，服务器端是无法收到客户端断开的通知的

bool CIOCPModel::_IsSocketAlive(SOCKET s)
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}

///////////////////////////////////////////////////////////////////
// 显示并处理完成端口上的错误
bool CIOCPModel::HandleError( PER_SOCKET_CONTEXT *pContext,const DWORD& dwErr )
{
	// 如果是超时了，就再继续等吧  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// 确认客户端是否还活着...
		if( !_IsSocketAlive( pContext->m_Socket) )
		{
			this->_ShowMessage( _T("检测到客户端异常退出！") );
			this->_RemoveContext( pContext );
			return true;
		}
		else
		{
			this->_ShowMessage( _T("网络操作超时！重试中...") );
			return true;
		}
	}  

	// 可能是客户端异常退出了
	else if( ERROR_NETNAME_DELETED==dwErr )
	{
		this->_ShowMessage( _T("检测到客户端异常退出！") );
		this->_RemoveContext( pContext );
		return true;
	}

	else
	{
		this->_ShowMessage( _T("完成端口操作出现错误，线程退出。错误代码：%d"),dwErr );
		return false;
	}
}
```



------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)