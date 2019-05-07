## 网络通信基础重难点解析 10 ：Linux EINTR 错误码

### Linux EINTR 错误码

在类 Unix 操作系统中（当然也包括 Linux 系统），当我们调用一些 socket 函数时（connect、send、recv、epoll_wait 等），除了函数调用出错会返回 **-1**，这些函数可能被信号中断也会返回 **-1**，此时我们可以通过错误码 **errno** 判断是不是 **EINTR ** 来确定是不是被信号中断。在实际编码的时候，请读者务必要考虑到这种情况。这也是上文中很多代码要专门判断错误码是不是 **EINTR**，如果是，说明被信号中断，我们需要再次调用这个函数进行重试。千万不要一看到返回值是 -1，草草认定这些调用失败，进而做出错误逻辑判断。

```
bool SendData(const char* buf , int buf_length)
{
    //已发送的字节数目
    int sent_bytes = 0;
    int ret = 0;
    while (true)
    {
        ret = send(m_hSocket, buf + sent_bytes, buf_length - sent_bytes, 0);
        if (nRet == -1)
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
        else if (nRet == 0)
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
```



------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)