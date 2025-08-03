# 1. netfilter子系统框架
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
#### 2.2.1.7 int (*okfn)(struct net *, struct sock *, struct sk_buff *)
“一切正常”的回调函数（"OK" Function），如果 Netfilter 钩子链中的所有规则都接受 (NF_ACCEPT) 数据包，并且数据包没有被其他钩子点处理或丢弃，那么 nf_hook 函数最终会调用这个 okfn 回调函数，将数据包递交给网络栈的下一个处理阶段

### 2.2.2 hook函数返回值
> include/uapi/linux/netfilter.h
```c
/* Responses from hook functions. */
#define NF_DROP 0
#define NF_ACCEPT 1
#define NF_STOLEN 2  // 不继续传输，由勾子函数进行处理
#define NF_QUEUE 3
#define NF_REPEAT 4
#define NF_STOP 5	/* Deprecated, for userspace nf_queue compatibility. */
#define NF_MAX_VERDICT NF_STOP
```
### 2.2.3 注册勾子函数
#### 2.2.3.1 定义nf_hook_ops对象
```c
struct nf_hook_ops {
	/* 用户填写 */
	nf_hookfn		*hook; // 要挂入的 钩子处理函数
	struct net_device	*dev;
	void			*priv;
	u8			pf;
	enum nf_hook_ops_type	hook_ops_type:8;
	unsigned int		hooknum; // hook 挂载点编号
	/* Hooks are ordered in ascending priority. */
	int			priority; //优先级（数字越小越早执行）
};
```
#### 2.2.3.2 注册钩子函数
```c
/* Function to register/unregister hook points. */
int nf_register_net_hook(struct net *net, const struct nf_hook_ops *ops);
void nf_unregister_net_hook(struct net *net, const struct nf_hook_ops *ops);
int nf_register_net_hooks(struct net *net, const struct nf_hook_ops *reg,
			  unsigned int n);
```
# 3. 连接跟踪
>net/netfilter/nf_conntrack_proto.c<br>
最重要的两个两个连接跟踪回调函数，NF_INET_PRE_ROUTING钩子回调函数ipv4_conntrack_in()和NF_INET_LOCAL_OUT钩子回调函数ipv4_conntrack_local()，优先级为NF_IP_PRI_CONNTRACK(-200)
## 3.1 连接跟踪回调函数
### 3.1.1 ipv4_conntrack_in
```c
static unsigned int ipv4_conntrack_in(void *priv,
				      struct sk_buff *skb,
				      const struct nf_hook_state *state)
{
	return nf_conntrack_in(skb, state);
}
```

```c
static const struct nf_hook_ops ipv4_conntrack_ops[] = {
	{
		.hook		= ipv4_conntrack_in,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP_PRI_CONNTRACK,
	},
	{
		.hook		= ipv4_conntrack_local,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP_PRI_CONNTRACK,
	},
	{
		.hook		= nf_confirm,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_POST_ROUTING,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM,
	},
	{
		.hook		= nf_confirm,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM,
	},
};
```
### 3.1.2 ipv4_conntrack_local
```c
static unsigned int ipv4_conntrack_local(void *priv,
					 struct sk_buff *skb,
					 const struct nf_hook_state *state)
{
	if (ip_is_fragment(ip_hdr(skb))) { /* IP_NODEFRAG setsockopt set */
		enum ip_conntrack_info ctinfo;
		struct nf_conn *tmpl;

		tmpl = nf_ct_get(skb, &ctinfo);
		if (tmpl && nf_ct_is_template(tmpl)) {
			/* when skipping ct, clear templates to avoid fooling
			 * later targets/matches
			 */
			skb->_nfct = 0;
			nf_ct_put(tmpl);
		}
		return NF_ACCEPT;
	}

	return nf_conntrack_in(skb, state);
}
```

### 3.1.3 连接跟踪的基本元素 struct nf_conntrack_tuple
> include/net/netfilter/nf_conntrack_tuple.h
```c
/* 这个结构体表示特定方向上的流， dst结构体包含各种协议对象*/
struct nf_conntrack_tuple {
	struct nf_conntrack_man src;

	/* These are the parts of the tuple which are fixed. */
	struct {
		union nf_inet_addr u3;
		union {
			/* Add other protocols here. */
			__be16 all;

			struct {
				__be16 port;
			} tcp;
			struct {
				__be16 port;
			} udp;
			struct {
				u_int8_t type, code;
			} icmp;
			struct {
				__be16 port;
			} dccp;
			struct {
				__be16 port;
			} sctp;
			struct {
				__be16 key;
			} gre;
		} u;

		/* The protocol. */
		u_int8_t protonum;

		/* The direction must be ignored for the tuplehash */
		struct { } __nfct_hash_offsetend;

		/* The direction (for tuplehash) */
		u_int8_t dir;
	} dst;
};
```
### 3.1.4 连接跟踪的基本条目 struct nf_conn
> include/net/netfilter/nf_conntrack.h
```c
struct nf_conn {
	/* Usage count in here is 1 for hash table, 1 per skb,
	 * plus 1 for any connection(s) we are `master' for
	 *
	 * Hint, SKB address this struct and refcnt via skb->_nfct and
	 * helpers nf_conntrack_get() and nf_conntrack_put().
	 * Helper nf_ct_put() equals nf_conntrack_put() by dec refcnt,
	 * except that the latter uses internal indirection and does not
	 * result in a conntrack module dependency.
	 * beware nf_ct_get() is different and don't inc refcnt.
	 */
	struct nf_conntrack ct_general;

	spinlock_t	lock;
	/* jiffies32 when this ct is considered dead */
	u32 timeout;

#ifdef CONFIG_NF_CONNTRACK_ZONES
	struct nf_conntrack_zone zone;
#endif
	/* XXX should I move this to the tail ? - Y.K */
	/* These are my tuples; original and reply */
	struct nf_conntrack_tuple_hash tuplehash[IP_CT_DIR_MAX];

	/* Have we seen traffic both ways yet? (bitset) */
	unsigned long status;

	possible_net_t ct_net;

#if IS_ENABLED(CONFIG_NF_NAT)
	struct hlist_node	nat_bysource;
#endif
	/* all members below initialized via memset */
	struct { } __nfct_init_offset;

	/* If we were expected by an expectation, this will be it */
	struct nf_conn *master;

#if defined(CONFIG_NF_CONNTRACK_MARK)
	u_int32_t mark;
#endif

#ifdef CONFIG_NF_CONNTRACK_SECMARK
	u_int32_t secmark;
#endif

	/* Extensions */
	struct nf_ct_ext *ext;

	/* Storage reserved for other modules, must be the last member */
	union nf_conntrack_proto proto;
};
```

> net/netfilter/nf_conntrack_core.c
```c
unsigned int
nf_conntrack_in(struct sk_buff *skb, const struct nf_hook_state *state)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct, *tmpl;
	u_int8_t protonum;
	int dataoff, ret;

	tmpl = nf_ct_get(skb, &ctinfo);
	if (tmpl || ctinfo == IP_CT_UNTRACKED) {
		/* Previously seen (loopback or untracked)?  Ignore. */
		if ((tmpl && !nf_ct_is_template(tmpl)) ||
		     ctinfo == IP_CT_UNTRACKED)
			return NF_ACCEPT;
		skb->_nfct = 0;
	}

	/* rcu_read_lock()ed by nf_hook_thresh */
	dataoff = get_l4proto(skb, skb_network_offset(skb), state->pf, &protonum);
	if (dataoff <= 0) {
		NF_CT_STAT_INC_ATOMIC(state->net, invalid);
		ret = NF_ACCEPT;
		goto out;
	}

	if (protonum == IPPROTO_ICMP || protonum == IPPROTO_ICMPV6) {
		ret = nf_conntrack_handle_icmp(tmpl, skb, dataoff,
					       protonum, state);
		if (ret <= 0) {
			ret = -ret;
			goto out;
		}
		/* ICMP[v6] protocol trackers may assign one conntrack. */
		if (skb->_nfct)
			goto out;
	}
repeat:
	ret = resolve_normal_ct(tmpl, skb, dataoff,
				protonum, state);
	if (ret < 0) {
		/* Too stressed to deal. */
		NF_CT_STAT_INC_ATOMIC(state->net, drop);
		ret = NF_DROP;
		goto out;
	}

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct) {
		/* Not valid part of a connection */
		NF_CT_STAT_INC_ATOMIC(state->net, invalid);
		ret = NF_ACCEPT;
		goto out;
	}

	ret = nf_conntrack_handle_packet(ct, skb, dataoff, ctinfo, state);
	if (ret <= 0) {
		/* Invalid: inverse of the return code tells
		 * the netfilter core what to do */
		nf_ct_put(ct);
		skb->_nfct = 0;
		/* Special case: TCP tracker reports an attempt to reopen a
		 * closed/aborted connection. We have to go back and create a
		 * fresh conntrack.
		 */
		if (ret == -NF_REPEAT)
			goto repeat;

		NF_CT_STAT_INC_ATOMIC(state->net, invalid);
		if (ret == NF_DROP)
			NF_CT_STAT_INC_ATOMIC(state->net, drop);

		ret = -ret;
		goto out;
	}

	if (ctinfo == IP_CT_ESTABLISHED_REPLY &&
	    !test_and_set_bit(IPS_SEEN_REPLY_BIT, &ct->status))
		nf_conntrack_event_cache(IPCT_REPLY, ct);
out:
	if (tmpl)
		nf_ct_put(tmpl);

	return ret;
}
```
