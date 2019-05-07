## 网络通信基础重难点解析 14 ：Windows 的 WSAAsyncSelect 网络通信模型



### Windows 的 WSAAsyncSelect 网络通信模型

**WSAAsyncSelect ** 是 Windows 系统非常常用一个网络通信模型，它的原理是将 socket 句柄绑定到一个 Windows 窗口上并利于 Windows 的窗口消息机制实现了网络有消息时调用窗口函数。**WSAAsyncSelect ** 函数签名如下：

```
int WSAAsyncSelect(
  SOCKET s,
  HWND   hWnd,
  u_int  wMsg,
  long   lEvent
);
```

参数 **s** 和 **hwnd** 是需要绑定在一起的 socket 句柄和窗口句柄，参数 **uMsg** 是自定义的一个窗口消息，socket 有事件时会产生这个消息类型，为了避免与 Windows 内置消息冲突，通常这个消息值应该在 WM_USER 基础之上定义（如 WM_USER + 1），参数 **lEvent** 即 要监听的 socket 事件类型，它的取值是上一小节介绍的 FD_XXX 系列。函数调用成功返回 0 值，调用失败返回 SOCKET_ERROR（-1）。

> **WSAAsyncSelect** 如果设置了 lEvent 值（非 0），会自动将参数 s 对应的 socket 设置为非阻塞模式；反之，如果设置 lEvent = 0 会自动将 socket 变回阻塞模式。



我们来看一个具体的示例代码：

```
// WSAAsyncSelect.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <winsock2.h>
#include "WSAAsyncSelect.h"

#pragma comment(lib, "ws2_32.lib")

//socket 消息
#define WM_SOCKET   WM_USER + 1

//当前在线用户数量
int    g_nCount = 0;

SOCKET              InitSocket();
ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT OnSocketEvent(HWND hWnd, WPARAM wParam, LPARAM lParam);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

    SOCKET hListenSocket = InitSocket();
    if (hListenSocket == INVALID_SOCKET)
        return 1;

	MSG msg;
	MyRegisterClass(hInstance);

    HWND hwnd = InitInstance(hInstance, nCmdShow);
    if (hwnd == NULL)
        return 1;

    //利用 WSAAsyncSelect 将侦听 socket 与 hwnd 绑定在一起
    if (WSAAsyncSelect(hListenSocket, hwnd, WM_SOCKET, FD_ACCEPT) == SOCKET_ERROR)
        return 1;

	while (GetMessage(&msg, NULL, 0, 0))
	{		
        TranslateMessage(&msg);
        DispatchMessage(&msg);	
	}

    closesocket(hListenSocket);
    WSACleanup();

	return (int) msg.wParam;
}

SOCKET InitSocket()
{
    //1. 初始化套接字库
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);
    int nError = WSAStartup(wVersionRequested, &wsaData);
    if (nError != 0)
        return INVALID_SOCKET;

    if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
    {
        WSACleanup();
        return INVALID_SOCKET;
    }

    //2. 创建用于监听的套接字
    SOCKET hListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN addrSrv;
    addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(6000);

    //3. 绑定套接字
    if (bind(hListenSocket, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        closesocket(hListenSocket);
        WSACleanup();
        return INVALID_SOCKET;
    }

    //4. 将套接字设为监听模式，准备接受客户请求
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(hListenSocket);
        WSACleanup();
        return INVALID_SOCKET;
    }

    return hListenSocket;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WSAASYNCSELECT));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= _T("DemoWindowCls");
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{  
   HWND hWnd = CreateWindow(_T("DemoWindowCls"), _T("DemoWindow"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
   if (!hWnd)
      return NULL;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return hWnd;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

    switch (uMsg)
	{
    case WM_SOCKET:
        return OnSocketEvent(hWnd, wParam, lParam);


	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

LRESULT OnSocketEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    SOCKET s = (SOCKET)wParam;
    int nEventType = WSAGETSELECTEVENT(lParam);
    int nErrorCode = WSAGETSELECTERROR(lParam);
    if (nErrorCode != 0)
        return 1;

    switch (nEventType)
    {
    case FD_ACCEPT:
    {
        //调用accept函数处理接受连接事件;
        SOCKADDR_IN addrClient;
        int len = sizeof(SOCKADDR);
        //等待客户请求到来
        SOCKET hSockClient = accept(s, (SOCKADDR*)&addrClient, &len);
        if (hSockClient != SOCKET_ERROR)
        {
            //产生的客户端socket，监听其 FD_READ/FD_CLOSE 事件
            if (WSAAsyncSelect(hSockClient, hWnd, WM_SOCKET, FD_READ | FD_CLOSE) == SOCKET_ERROR)
            {
                closesocket(hSockClient);
                return 1;
            }

            g_nCount++;  
            TCHAR szLogMsg[64];
            wsprintf(szLogMsg, _T("a client connected, socket: %d, current: %d\n"), (int)hSockClient, g_nCount);
            OutputDebugString(szLogMsg);
        }
    }
    break;

    case FD_READ:
    {
        char szBuf[64] = { 0 };
        int n = recv(s, szBuf, 64, 0);
        if (n > 0)
        {
            OutputDebugStringA(szBuf);
        }
        else if (n <= 0)
        {
            g_nCount--;
            TCHAR szLogMsg[64];
            wsprintf(szLogMsg, _T("a client disconnected, socket: %d, current: %d\n"), (int)s, g_nCount);
            OutputDebugString(szLogMsg);
            closesocket(s);
        }
    }
    break;

    case FD_CLOSE:
    {
        g_nCount--;
        TCHAR szLogMsg[64];
        wsprintf(szLogMsg, _T("a client disconnected, socket: %d, current: %d\n"), (int)s, g_nCount);
        OutputDebugString(szLogMsg);
        closesocket(s);
    }
    break;

    }// end switch

    return 0;
}
```

在 Visual Studio 中编译该程序，然后在另外一台 Linux 机器上使用 nc 命令模拟几个客户端，模拟命令如下：

```
# 我的服务器地址是 192.168.1.131
[root@localhost ~]# nc -v 192.168.1.131 6000
```

Windows 服务程序的输出是使用 OutputDebugString 函数来输出到 Visual Studio 的 **Output 窗口**中去的，所以需要在调试模式下运行服务程序。**Output 窗口** 输出效果如下：

![](http://www.hootina.org/github_easyserverdev/20190318221652.png)



上述代码中有几个地方需要注意：

- 当产生了 WM_SOCKET 消息时，消息携带的参数 wParam 的值是产生网络事件的 socket 句柄值，参数 lParam 分为两段，高 16 位（bit）（2 字节）是网络错误码（0 为没有错位），低 16 位（bit）（2 字节）是网络事件类型，Windows 专门为了取得这两个值分别定义了宏 **WSAGETSELECTERROR** 和 **WSAGETSELECTEVENT**。

  ```
  #define WSAGETSELECTEVENT(lParam)       LOWORD(lParam)
  #define WSAGETSELECTERROR(lParam)       HIWORD(lParam)
  ```

- 对于侦听 socket， 我们这里只关注其 FD_CONNECT 事件，对于普通 socket 我们关注其 FD_READ 和 FD_CLOSE 事件。 



>  mfc 中的 CAsyncSocket 类的实现就是基于 WSAAsyncSelect 这个函数封装的。



------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)