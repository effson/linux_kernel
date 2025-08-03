# netfilter子系统框架
## IPVS
IPVS（IP Virtual Server）是 Linux 内核中用于实现 四层（L4）负载均衡 的内核子系统，它是 LVS（Linux Virtual Server）项目 的核心组件。它提供一种高性能、高可用、基于 IP 层的负载均衡解决方案，它通过修改数据包的目标地址，将客户端请求分发到后端多个真实服务器（Real Servers）上，实现服务的扩展与容错
## IP sets
IP sets（IP 集合）是 Linux 内核中提供的一种 高效的 IP 匹配机制，它作为 iptables 的扩展模块使用，用于管理大量 IP 地址、端口、网络段、MAC 等的数据集合，并在防火墙规则中进行快速匹配。可以把它看成是“iptables 的数据库”——通过 match ip in set 来快速判断是否匹配规则
