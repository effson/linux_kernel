### 1.UDP报头
<img width="869" height="333" alt="image" src="https://github.com/user-attachments/assets/321b49e1-2de9-425a-8e3a-807b34d06fc0" />

### 2.内核源码
> include/uapi/linux/udp.h
```c
struct udphdr {
	__be16	source;
	__be16	dest;
	__be16	len;
	__sum16	check;
};
```

### 3.UDP初始化
> net/ipv4/af_inet.c
```c
static __net_init int inet_init_net(struct net *net)
{
	/*
	 * 把高 16 位当作上界、低 16 位当作下界编码，得到默认区间 32768–60999。
	 * 内核为主动连接/无绑定套接字选取本地源端口时就从这个区间里挑
	 */
	net->ipv4.ip_local_ports.range = 60999u << 16 | 32768u;

	/*ping 组范围（允许无 CAP_NET_RAW 创建 ICMP ping 的组）*/
	seqlock_init(&net->ipv4.ping_group_range.lock);
	net->ipv4.ping_group_range.range[0] = make_kgid(&init_user_ns, 1);
	net->ipv4.ping_group_range.range[1] = make_kgid(&init_user_ns, 0);

	/*  sysctl 默认值 */
	net->ipv4.sysctl_ip_default_ttl = IPDEFTTL;
	net->ipv4.sysctl_ip_fwd_update_priority = 1;
	net->ipv4.sysctl_ip_dynaddr = 0;
	net->ipv4.sysctl_ip_early_demux = 1;
	net->ipv4.sysctl_udp_early_demux = 1;
	net->ipv4.sysctl_tcp_early_demux = 1;
	net->ipv4.sysctl_nexthop_compat_mode = 1;
#ifdef CONFIG_SYSCTL
	net->ipv4.sysctl_ip_prot_sock = PROT_SOCK;
#endif

	/* IGMP（IPv4 组播）相关默认值 */
	net->ipv4.sysctl_igmp_max_memberships = 20;
	net->ipv4.sysctl_igmp_max_msf = 10;
	/* IGMP reports for link-local multicast groups are enabled by default */
	net->ipv4.sysctl_igmp_llm_reports = 1;
	net->ipv4.sysctl_igmp_qrv = 2;

	net->ipv4.sysctl_fib_notify_on_flag_change = 0;

	return 0;
}
```

```c
