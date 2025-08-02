## 1.Netlink 的基本概念
### 1.1 介绍
Netlink 是一种基于 socket 的 IPC（进程间通信）机制，通过一种类似 BSD socket 的方式，让用户空间程序可以向内核发送请求或接收内核的通知。
### 1.2 源码 
在代码层面，它是通过 AF_NETLINK 协议族实现的，使用的 API 是 socket()、bind()、sendmsg()、recvmsg() 等<br>
在 net/netlink源码目录下<br>
<img width="144" height="179" alt="image" src="https://github.com/user-attachments/assets/245aabc1-38e6-48c9-87fa-fb4621b713fb" />

事实上最常用的是af_netlink模块，它提供netlink内核套接字API，而genetlink模块提供了新的通用netlink API。监视接
口模块diag(diag.c)提供的API用于读写有关的netlink套接字信息。从理论原则来讲，netlink套接字可用于在用户空间进程间
通信（包括发送组播消息），但通常不这样做，而且这也不是开发netlink套接字初衷。UNIX域套接字提供用于IPC的API，被
广泛用于两个用户空间进行间的通信。
### 1.3 优势
使用netlink套接字时不需要轮询，用空间应用程序打开套接字，再调用recvmsg()，如果没有来自内核的消息，就进
入阻塞状态；内核可以主动向用户空间发送异步消息，而不需要用户空间来触发；netlink套接字支持组播传输。要在用户空间
中创建netlink套接字，可使用系统调用socket()，netlink套接字就可以是SOCK_RAW套接字，也可以是SOCK_DGRAM套接
字。
## 2.netlink数据结构源码
### 2.1 struct sockaddr_nl
> include/uapi/linux/netlink.h
```c
struct sockaddr_nl {
	__kernel_sa_family_t	nl_family;	/* AF_NETLINK	*/
	unsigned short	nl_pad;		/* zero		*/
	__u32		nl_pid;		/* port ID	*/
       	__u32		nl_groups;	/* multicast groups mask */
};
```
