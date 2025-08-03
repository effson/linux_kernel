# 1.netfilter子系统框架
## 1.1 IPVS
IPVS（IP Virtual Server）是 Linux 内核中用于实现 四层（L4）负载均衡 的内核子系统，它是 LVS（Linux Virtual Server）项目 的核心组件。它提供一种高性能、高可用、基于 IP 层的负载均衡解决方案，它通过修改数据包的目标地址，将客户端请求分发到后端多个真实服务器（Real Servers）上，实现服务的扩展与容错
## 1.2 IP sets
IP sets（IP 集合）是 Linux 内核中提供的一种 高效的 IP 匹配机制，它作为 iptables 的扩展模块使用，用于管理大量 IP 地址、端口、网络段、MAC 等的数据集合，并在防火墙规则中进行快速匹配。可以把它看成是“iptables 的数据库”——通过 match ip in set 来快速判断是否匹配规则
## 1.3 iptables
iptables 是 Linux 系统中用户空间管理防火墙规则的命令行工具，配合内核的 Netfilter 框架，用于实现 数据包过滤、防火墙、NAT 转发、流量控制 等功能。
iptables 是 Linux 内核 Netfilter 子系统的前端接口，用来定义数据包在内核协议栈中的处理策略（放行、丢弃、转发、改写等）。
可以添加和删除Netfilter规则、显示统计信息、添加表、将表中计数器重置为0等等。

# 2.Netfilter挂载点
## 2.1 5个Netfilter挂载点
> include/uapi/linux/netfilter.h<br>
在网络栈有5个地方设置Netfilter挂载点，具体源码解析可以参考网络层的md资料：
```c
enum nf_inet_hooks {
	NF_INET_PRE_ROUTING,
	NF_INET_LOCAL_IN,
	NF_INET_FORWARD,
	NF_INET_LOCAL_OUT,
	NF_INET_POST_ROUTING,
	NF_INET_NUMHOOKS,
	NF_INET_INGRESS = NF_INET_NUMHOOKS,
};
```
### 2.1 NF_INET_PRE_ROUTING
在IPv4中，这个挂接点方法为ip_rcv(),在IPv6中为ipv6_rcv(),所有的入栈数据包遇到的第一个挂载点，处于路由选择子系统查找之前。
### 2.2 NF_INET_LOCAL_IN
在IPv4中，这个挂接点方法为ip_local_deliver(),在IPv6中为ip6_input(),所有发送到当前主机的数据包，经过NF_INET_PRE_ROUTING并执行路由选择子系统查找后，都将到达这个挂载点
### 2.3 NF_INET_FORWARD
在IPv4中，这个挂接点方法为ip_forward(),在IPv6中为ip6_forward(),所有要转发的数据包，经过NF_INET_PRE_ROUTING并执行路由选择子系统查找后，都将到达这个挂载点
### 2.4 NF_INET_POST_ROUTING
在IPv4中，这个挂接点方法为ip_output(),在IPv6中为ip6_finish_output2(),所有要转发的数据包，经过NF_INET_FORWARD后，都将到达这个挂载点;主机生成的数据包经过NF_INET_LOCAL_OUT后将到达这个挂接点
### 2.5 NF_INET_LOCAL_OUT
在IPv4中，这个挂接点方法为__ip_local_out(),在IPv6中为ip6_local_out(),所有要转发的数据包，主机生成的数据包经过这个挂载点后将到达NF_INET_POST_ROUTING挂接点
## 2.2 内核源码分析
### 2.2.1 NF_HOOK
> include/linux/netfilter.h
```c
static inline int
NF_HOOK(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk,
	struct sk_buff *skb, struct net_device *in, struct net_device *out,
	int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	return okfn(net, sk, skb);
}
```

```c
static inline int nf_hook(u_int8_t pf, unsigned int hook, struct net *net,
			  struct sock *sk, struct sk_buff *skb,
			  struct net_device *indev, struct net_device *outdev,
			  int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	return 1;
}
```
#### 2.2.1.1 u_int8_t pf
- NFPROTO_IPV4 (或 AF_INET): IPv4 协议。
- NFPROTO_IPV6 (或 AF_INET6): IPv6 协议。
- NFPROTO_ARP: ARP 协议。
- NFPROTO_BRIDGE: 桥接协议（用于二层处理）
#### 2.2.1.2 unsigned int hook
- NF_INET_PRE_ROUTING: 数据包进入网络设备后，路由判断之前。
- NF_INET_LOCAL_IN: 数据包被判断为发往本地进程后。
- NF_INET_FORWARD: 数据包被判断为需要转发时，在转发到下一跳之前。
- NF_INET_LOCAL_OUT: 本地进程生成的数据包，在路由判断之后、发送之前。
- NF_INET_POST_ROUTING: 数据包即将离开网络设备之前（例如，源 NAT 会在这里发生）。
#### 2.2.1.3 struct net *net
网络命名空间（Network Namespace），Linux 网络栈支持多个 netns（容器场景常见）
#### 2.2.1.4 struct sock *sk
关联的套接字（Socket），如果数据包是由本地进程的套接字生成（出站）或发往本地进程的套接字（入站），这个指针会指向相关的 struct sock 结构体。对于转发的数据包，通常为 NULL。它允许 Netfilter 规则检查套接字相关的信息
#### 2.2.1.5 struct sk_buff *skb
正在处理的数据包，Netfilter 所有钩子都处理它。可以查看/修改它的 IP 头、TCP 头、payload 等
#### 2.2.1.6 struct net_device *indev, struct net_device *outdev
入站网络设备（Inbound Network Device）和 出站网络设备（Outbound Network Device）。
