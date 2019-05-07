## 网络通信基础重难点解析 16 ：域名解析 API 介绍

为了便于记忆，有时候我们需要我们的程序可以使用域名和端口号去连接服务，这种情况下，我们需要使用 socket API **gethostbyname** 函数先把域名转换成 ip 地址，再使用 connect 函数连接。在 Linux 系统上， **gethostbyname** 函数的签名如下：

```
#include <netdb.h>
       
struct hostent* gethostbyname(const char* name);
```

域名转换成 ip 时，转换结果存在一个 **hostent** 结构体中。转换成功后的 ip 地址存放在 **hostent** 最后一个字段中，**hostent** 结构体类型定义如下：

```
struct hostent 
{
    char*  h_name;            /* official name of host */
    char** h_aliases;         /* alias list */
    int    h_addrtype;        /* host address type */
    int    h_length;          /* length of address */
    char** h_addr_list;       /* list of addresses */
}

#define h_addr h_addr_list[0] /* for backward compatibility */
```

- 字段 **h_name**： 地址的正式名称；
- 字段 **h_aliases**： 地址的预备名称指针；
- 字段 **h_addrtype**： 地址类型，通常是AF_INET；  
- 字段 **h_length**： 地址的长度，以字节数目为计量单位；
- 字段 **h_addr_list**：主机网络地址指针，网络字节顺序。 其中，**h_addr** 是字段 **h_addr_list** 中的第一地址。 

**注意**：虽然 **h_addr_list[0]** 看起来是一个 **char*** 类型，但实际上是一个 **uint32_t**，这是 ip 地址的 32 bit 整数表示形式，如果需要转换成十进制点分法字符串再调用 inet_ntoa() 函数即可。

我们来看一段示例代码：

```
#include <sys/types.h>  
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>

//extern int h_errno;

bool connect_to_server(const char* server, short port)
{
    int hSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (hSocket == -1)
        return false;

    struct sockaddr_in addrSrv = { 0 };
    struct hostent* pHostent = NULL;
    //unsigned int addr = 0;

    //如果传入的参数 server 的值是 somesite.com 这种域名域名形式则 if 条件成立，
	//接着调用 gethostbyname 解析域名为 4 字节的 ip 地址（整型）
	if (addrSrv.sin_addr.s_addr = inet_addr(server) == INADDR_NONE)
    {       
		pHostent = gethostbyname(server);
        if (pHostent == NULL)      
            return false;
        
        //当使用 gethostbyname 解析域名时可能会得到多个 ip 地址，一般最常用的使用第一个 ip 地址
        addrSrv.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr_list[0]);
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(port);
    int ret = connect(hSocket, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
    if (ret == -1)
        return false;

    return true;
}

int main()
{
	if (connect_to_server("baidu.com", 80))
		printf("connect successfully.\n");
	else
		printf("connect error.\n");
	
	return 0;
}
```

上述 **connect_to_server** 函数既可以支持直接传入域名，也可以传入 ip 地址：

```
connect_to_server("127.0.0.1", 8888);
connect_to_server("localhost", 8888);

connect_to_server("61.135.169.125", 80);
connect_to_server("baidu.com", 80);
```

实际在使用 **gethostbyname** 函数时需要注意以下：

- **gethostbyname**  函数是不可重入函数，在 Linux 下建议使用 **gethostbyname_r** 函数替代；

- **gethostbyname** 在解析域名时，会阻塞当前执行线程的，直到得到返回结果；

- 在使用 **gethostbyname** 函数出错时，你不能使用 errno 获取错误码信息（因此也不能使用 **perror()** 函数打印错误信息），你应该使用 **h_errno** 错误码（也可以调用 **herror()** 打印错误信息），**herror()** 函数签名如下：

  ```
  void herror(const char *s);
  ```



在新的 Linux 系统中，gethostbyname 和 gethostbyaddr 一样，已经被标记为废弃的，你应该使用新的函数 **getaddrinfo** 去替代它们，**getaddrinfo** 签名如下：

```
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char* node, 
                const char* service, 
                const struct addrinfo* hints, 
                struct addrinfo** res);
```

**getaddrinfo** 函数调用成功返回 0，失败返回非 0 值，调用成功后结果存储在参数 **res** 中。**addrinfo** 结构体定义如下：

```
struct addrinfo
{
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr* ai_addr;
    char*		     ai_canonname;
    struct addrinfo* ai_next;
};
```

如果你不再需要 **res** 这个变量，记得使用 **freeaddrinfo** 函数将其指向的资源释放：

```
void freeaddrinfo(struct addrinfo* res);
```

**getaddrinfo** 使用示例如下：

```
struct addrinfo hints = {0}; 
hints.ai_flags = AI_CANONNAME;
hints.ai_family = family;
hints.ai_socktype = socktype;

struct addrinfo* res;
int n = getaddrinfo(host, service, &hints, &res);
if(n == 0)
{
    //调用成功，使用 res
    
    //释放 res 资源
    freeaddr(res);
}
```



**getaddrinfo** 函数不仅支持 ipv4，同时也支持 ipv6 的解析，redis-server 的源码中就使用了这个函数去解析 ipv4 和 ipv6 的地址（位于 **net.c** 文件中）：

```
static int _redisContextConnectTcp(redisContext *c, const char *addr, int port,
                                   const struct timeval *timeout,
                                   const char *source_addr) {
    int s, rv, n;
    char _port[6];  /* strlen("65535"); */
    struct addrinfo hints, *servinfo, *bservinfo, *p, *b;
    int blocking = (c->flags & REDIS_BLOCK);
    int reuseaddr = (c->flags & REDIS_REUSEADDR);
    int reuses = 0;
    long timeout_msec = -1;

    servinfo = NULL;
    c->connection_type = REDIS_CONN_TCP;
    c->tcp.port = port;

    /* We need to take possession of the passed parameters
     * to make them reusable for a reconnect.
     * We also carefully check we don't free data we already own,
     * as in the case of the reconnect method.
     *
     * This is a bit ugly, but atleast it works and doesn't leak memory.
     **/
    if (c->tcp.host != addr) {
        if (c->tcp.host)
            free(c->tcp.host);

        c->tcp.host = strdup(addr);
    }

    if (timeout) {
        if (c->timeout != timeout) {
            if (c->timeout == NULL)
                c->timeout = malloc(sizeof(struct timeval));

            memcpy(c->timeout, timeout, sizeof(struct timeval));
        }
    } else {
        if (c->timeout)
            free(c->timeout);
        c->timeout = NULL;
    }

    if (redisContextTimeoutMsec(c, &timeout_msec) != REDIS_OK) {
        __redisSetError(c, REDIS_ERR_IO, "Invalid timeout specified");
        goto error;
    }

    if (source_addr == NULL) {
        free(c->tcp.source_addr);
        c->tcp.source_addr = NULL;
    } else if (c->tcp.source_addr != source_addr) {
        free(c->tcp.source_addr);
        c->tcp.source_addr = strdup(source_addr);
    }

    snprintf(_port, 6, "%d", port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* Try with IPv6 if no IPv4 address was found. We do it in this order since
     * in a Redis client you can't afford to test if you have IPv6 connectivity
     * as this would add latency to every connect. Otherwise a more sensible
     * route could be: Use IPv6 if both addresses are available and there is IPv6
     * connectivity. */
    if ((rv = getaddrinfo(c->tcp.host,_port,&hints,&servinfo)) != 0) {
         hints.ai_family = AF_INET6;
         if ((rv = getaddrinfo(addr,_port,&hints,&servinfo)) != 0) {
            __redisSetError(c,REDIS_ERR_OTHER,gai_strerror(rv));
            return REDIS_ERR;
        }
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
addrretry:
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;

        c->fd = s;
        if (redisSetBlocking(c,0) != REDIS_OK)
            goto error;
        if (c->tcp.source_addr) {
            int bound = 0;
            /* Using getaddrinfo saves us from self-determining IPv4 vs IPv6 */
            if ((rv = getaddrinfo(c->tcp.source_addr, NULL, &hints, &bservinfo)) != 0) {
                char buf[128];
                snprintf(buf,sizeof(buf),"Can't get addr: %s",gai_strerror(rv));
                __redisSetError(c,REDIS_ERR_OTHER,buf);
                goto error;
            }

            if (reuseaddr) {
                n = 1;
                if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*) &n,
                               sizeof(n)) < 0) {
                    goto error;
                }
            }

            for (b = bservinfo; b != NULL; b = b->ai_next) {
                if (bind(s,b->ai_addr,b->ai_addrlen) != -1) {
                    bound = 1;
                    break;
                }
            }
            freeaddrinfo(bservinfo);
            if (!bound) {
                char buf[128];
                snprintf(buf,sizeof(buf),"Can't bind socket: %s",strerror(errno));
                __redisSetError(c,REDIS_ERR_OTHER,buf);
                goto error;
            }
        }
        if (connect(s,p->ai_addr,p->ai_addrlen) == -1) {
            if (errno == EHOSTUNREACH) {
                redisContextCloseFd(c);
                continue;
            } else if (errno == EINPROGRESS && !blocking) {
                /* This is ok. */
            } else if (errno == EADDRNOTAVAIL && reuseaddr) {
                if (++reuses >= REDIS_CONNECT_RETRIES) {
                    goto error;
                } else {
                    redisContextCloseFd(c);
                    goto addrretry;
                }
            } else {
                if (redisContextWaitReady(c,timeout_msec) != REDIS_OK)
                    goto error;
            }
        }
        if (blocking && redisSetBlocking(c,1) != REDIS_OK)
            goto error;
        if (redisSetTcpNoDelay(c) != REDIS_OK)
            goto error;

        c->flags |= REDIS_CONNECTED;
        rv = REDIS_OK;
        goto end;
    }
    if (p == NULL) {
        char buf[128];
        snprintf(buf,sizeof(buf),"Can't create socket: %s",strerror(errno));
        __redisSetError(c,REDIS_ERR_OTHER,buf);
        goto error;
    }

error:
    rv = REDIS_ERR;
end:
    freeaddrinfo(servinfo);
    return rv;  // Need to return REDIS_OK if alright
}
```





------

**本文首发于『<font color=red>easyserverdev</font>』公众号，欢迎关注，转载请保留版权信息。**

**欢迎加入高性能服务器开发 QQ 群一起交流：<font color=red> 578019391 </font>。**

![微信扫码关注](http://www.hootina.org/github_easyserverdev/articlelogo.jpg)