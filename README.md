# 网络通信基础重难点解析 专题介绍

不积跬步无以至千里，不积小流无以成江海。

当我们了解了网络通信的基本原理后，你需要实际去编写一些网络通信程序，随着技术的更新换代、大浪淘沙，目前主要的网络通信技术都是基于 TCP/IP 协议栈的，对应到应用层的编码来说就是使用操作系统提供的 socket API 来编写网络通信程序。然而遗憾的是，拜各种网络库和开发 IDE 所赐，很多开发者或者网络编程的初学者都忽视了对这些基础的 socket API 的掌握。殊不知，操作系统层面提供的 API 会在相当长的时间内保持接口不变，一旦学成，终生受用。理解和掌握这些基础 socket API 不仅可以最大化地去定制各种网络通信框架，更不用说使用市面上流行的网络通信库了，最重要的是，它会是你排查各种网络疑难杂症坚实的技术保障。



### 文章目录

[网络通信基础重难点解析 01：常用 socket 函数基础](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2001%EF%BC%9A%E5%B8%B8%E7%94%A8%20socket%20%E5%87%BD%E6%95%B0%E5%9F%BA%E7%A1%80.md)

[网络通信基础重难点解析 02：TCP 通信基本流程](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2002%EF%BC%9ATCP%20%E9%80%9A%E4%BF%A1%E5%9F%BA%E6%9C%AC%E6%B5%81%E7%A8%8B.md)

[网络通信基础重难点解析 03：bind 函数](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2003%EF%BC%9Abind%20%E5%87%BD%E6%95%B0.md)

[网络通信基础重难点解析 04 ：select 函数用法](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2004%20%EF%BC%9Aselect%20%E5%87%BD%E6%95%B0%E7%94%A8%E6%B3%95.md)

[网络通信基础重难点解析 05 ：socket 的阻塞模式和非阻塞模式](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2005%20%EF%BC%9Asocket%20%E7%9A%84%E9%98%BB%E5%A1%9E%E6%A8%A1%E5%BC%8F%E5%92%8C%E9%9D%9E%E9%98%BB%E5%A1%9E%E6%A8%A1%E5%BC%8F.md)

[网络通信基础重难点解析 06 ：send 和 recv 函数在阻塞和非阻塞模式下的行为](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2006%20%EF%BC%9Asend%20%E5%92%8C%20recv%20%E5%87%BD%E6%95%B0%E5%9C%A8%E9%98%BB%E5%A1%9E%E5%92%8C%E9%9D%9E%E9%98%BB%E5%A1%9E%E6%A8%A1%E5%BC%8F%E4%B8%8B%E7%9A%84%E8%A1%8C%E4%B8%BA.md)

[网络通信基础重难点解析 07 ：非阻塞模式下 send 和 recv 函数的返回值总结](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2007%20%EF%BC%9A%E9%9D%9E%E9%98%BB%E5%A1%9E%E6%A8%A1%E5%BC%8F%E4%B8%8B%20send%20%E5%92%8C%20recv%20%E5%87%BD%E6%95%B0%E7%9A%84%E8%BF%94%E5%9B%9E%E5%80%BC%E6%80%BB%E7%BB%93.md)

[网络通信基础重难点解析 08 ：connect 函数在阻塞和非阻塞模式下的行为](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2008%20%EF%BC%9Aconnect%20%E5%87%BD%E6%95%B0%E5%9C%A8%E9%98%BB%E5%A1%9E%E5%92%8C%E9%9D%9E%E9%98%BB%E5%A1%9E%E6%A8%A1%E5%BC%8F%E4%B8%8B%E7%9A%84%E8%A1%8C%E4%B8%BA.md)

[网络通信基础重难点解析 09 ：阻塞与非阻塞的 socket 的各自适用场景](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2009%20%EF%BC%9A%E9%98%BB%E5%A1%9E%E4%B8%8E%E9%9D%9E%E9%98%BB%E5%A1%9E%E7%9A%84%20socket%20%E7%9A%84%E5%90%84%E8%87%AA%E9%80%82%E7%94%A8%E5%9C%BA%E6%99%AF.md)

[网络通信基础重难点解析 10 ：Linux EINTR 错误码](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2010%20%EF%BC%9ALinux%20EINTR%20%E9%94%99%E8%AF%AF%E7%A0%81.md)

[网络通信基础重难点解析 11 ：Linux poll 函数用法](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2011%20%EF%BC%9ALinux%20poll%20%E5%87%BD%E6%95%B0%E7%94%A8%E6%B3%95.md)

[网络通信基础重难点解析 12 ：Linux epoll 模型](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2012%20%EF%BC%9ALinux%20epoll%20%E6%A8%A1%E5%9E%8B.md)

[网络通信基础重难点解析 13 ：Windows WSAEventSelect 网络通信模型](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2013%20%EF%BC%9AWindows%20WSAEventSelect%20%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E6%A8%A1%E5%9E%8B.md)

[网络通信基础重难点解析 14 ：Windows 的 WSAAsyncSelect 网络通信模型](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2014%20%EF%BC%9AWindows%20%E7%9A%84%20WSAAsyncSelect%20%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E6%A8%A1%E5%9E%8B.md)

[网络通信基础重难点解析 15 ：主机字节序和网络字节序](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2015%20%EF%BC%9A%E4%B8%BB%E6%9C%BA%E5%AD%97%E8%8A%82%E5%BA%8F%E5%92%8C%E7%BD%91%E7%BB%9C%E5%AD%97%E8%8A%82%E5%BA%8F.md)

[网络通信基础重难点解析 16 ：域名解析 API 介绍](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2016%20%EF%BC%9A%E5%9F%9F%E5%90%8D%E8%A7%A3%E6%9E%90%20API%20%E4%BB%8B%E7%BB%8D.md)

[网络通信基础重难点解析 17 ：Windows 完成端口（IOCP）模型重难点解析](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2017%20%EF%BC%9AWindows%20%E5%AE%8C%E6%88%90%E7%AB%AF%E5%8F%A3%EF%BC%88IOCP%EF%BC%89%E6%A8%A1%E5%9E%8B%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90.md)

[网络通信基础重难点解析 18： IOCP实例 - gh0st源码分析（以网络通信模块为重点）](https://github.com/balloonwj/easyserverdev/blob/master/%E7%BD%91%E7%BB%9C%E9%80%9A%E4%BF%A1%E5%9F%BA%E7%A1%80%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90%2017%20%EF%BC%9AWindows%20%E5%AE%8C%E6%88%90%E7%AB%AF%E5%8F%A3%EF%BC%88IOCP%EF%BC%89%E6%A8%A1%E5%9E%8B%E9%87%8D%E9%9A%BE%E7%82%B9%E8%A7%A3%E6%9E%90.md)





------

**本专题文章来源于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载或 fork 请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)
