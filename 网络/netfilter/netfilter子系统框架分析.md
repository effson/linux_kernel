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
> include/uapi/linux/netfilter.h<br>
在网络栈有5个地方设置Netfilter挂载点：
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
