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
## 2.netlink内核源码
### 2.1 struct sockaddr_nl
> include/uapi/linux/netlink.h
```c
/* Netlink socket 地址结构，它在 Netlink 通信中用于标识一个通信端点（用户进程或内核）*/
struct sockaddr_nl {
	__kernel_sa_family_t	nl_family;	/* AF_NETLINK	*/
	unsigned short	nl_pad;		/* zero	保留字段，必须填 0	*/
	__u32		nl_pid;		/* port ID  端口号，通常为进程 PID，内核端使用 0	*/
       	__u32		nl_groups;	/* multicast groups mask */
};
```
### 2.2 创建netlink套接字
> include/linux/netlink.h
```c
static inline struct sock *
netlink_kernel_create(struct net *net, int unit, struct netlink_kernel_cfg *cfg)
{
	return __netlink_kernel_create(net, unit, THIS_MODULE, cfg);
}
```

```c
static int rtnl_unregister(int protocol, int msgtype)
{
	struct rtnl_link __rcu **tab;
	struct rtnl_link *link;
	int msgindex;

	BUG_ON(protocol < 0 || protocol > RTNL_FAMILY_MAX);
	msgindex = rtm_msgindex(msgtype);

	rtnl_lock();
	tab = rtnl_dereference(rtnl_msg_handlers[protocol]);
	if (!tab) {
		rtnl_unlock();
		return -ENOENT;
	}

	link = rcu_replace_pointer_rtnl(tab[msgindex], NULL);
	rtnl_unlock();

	kfree_rcu(link, rcu);

	return 0;
}
```
### 2.3 数据包的发送
> net/netlink/af_netlink.c
```c
int nlmsg_notify(struct sock *sk, struct sk_buff *skb, u32 portid,
		 unsigned int group, int report, gfp_t flags)
{
	int err = 0;

	if (group) {
		int exclude_portid = 0;

		if (report) {
			refcount_inc(&skb->users);
			exclude_portid = portid;
		}

		/* errors reported via destination sk->sk_err, but propagate
		 * delivery errors if NETLINK_BROADCAST_ERROR flag is set */
		err = nlmsg_multicast(sk, skb, exclude_portid, group, flags);
		if (err == -ESRCH)
			err = 0;
	}

	if (report) {
		int err2;

		err2 = nlmsg_unicast(sk, skb, portid);
		if (!err)
			err = err2;
	}

	return err;
}
```
### 2.4 netlink消息报头
>include/uapi/linux/netlink.h
```c
struct nlmsghdr {
	__u32		nlmsg_len; // 消息总长度
	__u16		nlmsg_type; //消息类型（如 RTM_NEWLINK, RTM_GETROUTE，或用户自定义）
	__u16		nlmsg_flags; // NLM_F_REQUEST、NLM_F_ACK、NLM_F_DUMP、NLM_F_MULTI...
	__u32		nlmsg_seq; // 序列号，用于匹配请求与响应
	__u32		nlmsg_pid; // 消息发送者 PID（用户空间一般为 getpid()，内核为 0）
};
```

### 2.5 netlink属性头
>include/uapi/linux/netlink.h
```c
/*
 *  <------- NLA_HDRLEN ------> <-- NLA_ALIGN(payload)-->
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 * |        Header       | Pad |     Payload       | Pad |
 * |   (struct nlattr)   | ing |                   | ing |
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 *  <-------------- nlattr->nla_len -------------->
 */

struct nlattr {
	__u16           nla_len;
	__u16           nla_type;
};
```
### 2.6 创建和发送通用Netlink消息
>include/uapi/linux/genetlink.h
```c
// Generic Netlink 特有头
struct genlmsghdr {
	__u8	cmd; //子命令号（由你自己定义）
	__u8	version;// 协议版本
	__u16	reserved;// 保留位，必须为0
};
```
```c
/**
 向默认网络命名空间（init_net）中某个 Generic Netlink 多播组广播一条 Netlink 消息（skb），
 通知所有已加入该组的用户空间监听者
 * genlmsg_multicast - multicast a netlink message to the default netns
 * @family: the generic netlink family
 * @skb: netlink message as socket buffer
 * @portid: own netlink portid to avoid sending to yourself
 * @group: offset of multicast group in groups array
 * @flags: allocation flags
 */
static inline int genlmsg_multicast(const struct genl_family *family,
				    struct sk_buff *skb, u32 portid,
				    unsigned int group, gfp_t flags)
{
	return genlmsg_multicast_netns(family, &init_net, skb,
				       portid, group, flags);
}
/* 发送到所有命名空间
 */
int genlmsg_multicast_allns(const struct genl_family *family,
			    struct sk_buff *skb, u32 portid,
			    unsigned int group);
```

### 2.7 套接字监视接口
sock_diag 是 Linux 内核中的一种 Netlink socket 监控机制，用于在用户空间查询和监控内核中的 socket 状态，包括：<br>
- TCP、UDP 套接字状态（如 ESTABLISHED、TIME_WAIT 等）
- Unix 域套接字状态
- 内核 socket 信息（如 IP、端口、队列大小、PID 绑定等）

> net/core/sock_diag.c<br>
创建源码：
```c
static int __net_init diag_net_init(struct net *net)
{
	struct netlink_kernel_cfg cfg = {
		.groups	= SKNLGRP_MAX,
		.input	= sock_diag_rcv,
		.bind	= sock_diag_bind,
		.flags	= NL_CFG_F_NONROOT_RECV,
	};

	net->diag_nlsk = netlink_kernel_create(net, NETLINK_SOCK_DIAG, &cfg);
	return net->diag_nlsk == NULL ? -ENOMEM : 0;
}
```

## 3.控制TCP/IP联网的用户空间包 (iproute2/net-tools)
> iproute2（ip 命令）现代主流网络管理工具，替代传统 ifconfig和route，如：ip addr, ip route, ip link等，使用 Netlink 与内核通信
#### 3.1 ip
ip 命令是 Linux 中用于配置和查看 TCP/IP 网络的重要工具，管理网络表和网络接口，属于 iproute2 工具包
#### 3.2 tc
tc（Traffic Control）命令是 Linux 中用于配置 流量控制（QoS） 的强大工具，属于 iproute2 工具集。
#### 3.3 ss
ss（Socket Stat）命令是 Linux 中用于查看当前系统中 socket（套接字）连接信息 的工具，功能类似旧的 netstat，但更快、信息更全面
#### 3.4 lnstat
lnstat 是 Linux 系统中的一个 网络统计工具，用于显示内核中各种 网络子系统统计表的内容
#### 3.5 bridge
bridge 是 Linux 中的一个命令行工具，用于配置和管理 Linux 内核中的桥接（Bridge）网络设备。它是 iproute2 套件的一部分，专门用于处理以太网网桥相关的操作
> net-tools
#### 3.6 ifconfig
#### 3.7 arp
#### 3.8 route
#### 3.9 netstat
#### 3.10 hostname
