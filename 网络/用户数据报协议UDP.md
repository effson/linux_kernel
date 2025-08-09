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
	net_hotdata.udp_protocol = (struct net_protocol) {
		.handler =	udp_rcv,
		.err_handler =	udp_err,
		.no_policy =	1,
	};
	if (inet_add_protocol(&net_hotdata.udp_protocol, IPPROTO_UDP) < 0)
		pr_crit("%s: Cannot add UDP protocol\n", __func__);
```

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
### 发送UDP数据包
> net/ipv4/udp.c
从UDP用户空间套接字中发送数据，可使用系统调用send()、sendto()、sendmsg()和write()，最终都会通过内核udp_sendmsg()方法处理
```c
int udp_sendmsg(struct sock *sk, struct msghdr *msg, size_t len)
{
	struct inet_sock *inet = inet_sk(sk);
	struct udp_sock *up = udp_sk(sk);
	DECLARE_SOCKADDR(struct sockaddr_in *, usin, msg->msg_name);
	struct flowi4 fl4_stack;
	struct flowi4 *fl4;
	int ulen = len;
	struct ipcm_cookie ipc;
	struct rtable *rt = NULL;
	int free = 0;
	int connected = 0;
	__be32 daddr, faddr, saddr;
	u8 scope;
	__be16 dport;
	int err, is_udplite = IS_UDPLITE(sk);
	int corkreq = udp_test_bit(CORK, sk) || msg->msg_flags & MSG_MORE;
	int (*getfrag)(void *, char *, int, int, int, struct sk_buff *);
	struct sk_buff *skb;
	struct ip_options_data opt_copy;
	int uc_index;

	if (len > 0xFFFF)
		return -EMSGSIZE;

	/*
	 *	Check the flags.
	 */

	if (msg->msg_flags & MSG_OOB) /* Mirror BSD error message compatibility */
		return -EOPNOTSUPP;

	getfrag = is_udplite ? udplite_getfrag : ip_generic_getfrag;

	fl4 = &inet->cork.fl.u.ip4;
	if (READ_ONCE(up->pending)) {
		/*
		 * There are pending frames.
		 * The socket lock must be held while it's corked.
		 */
		lock_sock(sk);
		if (likely(up->pending)) {
			if (unlikely(up->pending != AF_INET)) {
				release_sock(sk);
				return -EINVAL;
			}
			goto do_append_data;
		}
		release_sock(sk);
	}
	ulen += sizeof(struct udphdr);

	/*
	 *	Get and verify the address.
	 */
	if (usin) {
		if (msg->msg_namelen < sizeof(*usin))
			return -EINVAL;
		if (usin->sin_family != AF_INET) {
			if (usin->sin_family != AF_UNSPEC)
				return -EAFNOSUPPORT;
		}

		daddr = usin->sin_addr.s_addr;
		dport = usin->sin_port;
		if (dport == 0)
			return -EINVAL;
	} else {
		if (sk->sk_state != TCP_ESTABLISHED)
			return -EDESTADDRREQ;
		daddr = inet->inet_daddr;
		dport = inet->inet_dport;
		/* Open fast path for connected socket.
		   Route will not be used, if at least one option is set.
		 */
		connected = 1;
	}

	ipcm_init_sk(&ipc, inet);
	ipc.gso_size = READ_ONCE(up->gso_size);

	if (msg->msg_controllen) {
		err = udp_cmsg_send(sk, msg, &ipc.gso_size);
		if (err > 0) {
			err = ip_cmsg_send(sk, msg, &ipc,
					   sk->sk_family == AF_INET6);
			connected = 0;
		}
		if (unlikely(err < 0)) {
			kfree(ipc.opt);
			return err;
		}
		if (ipc.opt)
			free = 1;
	}
	if (!ipc.opt) {
		struct ip_options_rcu *inet_opt;

		rcu_read_lock();
		inet_opt = rcu_dereference(inet->inet_opt);
		if (inet_opt) {
			memcpy(&opt_copy, inet_opt,
			       sizeof(*inet_opt) + inet_opt->opt.optlen);
			ipc.opt = &opt_copy.opt;
		}
		rcu_read_unlock();
	}

	if (cgroup_bpf_enabled(CGROUP_UDP4_SENDMSG) && !connected) {
		err = BPF_CGROUP_RUN_PROG_UDP4_SENDMSG_LOCK(sk,
					    (struct sockaddr *)usin,
					    &msg->msg_namelen,
					    &ipc.addr);
		if (err)
			goto out_free;
		if (usin) {
			if (usin->sin_port == 0) {
				/* BPF program set invalid port. Reject it. */
				err = -EINVAL;
				goto out_free;
			}
			daddr = usin->sin_addr.s_addr;
			dport = usin->sin_port;
		}
	}

	saddr = ipc.addr;
	ipc.addr = faddr = daddr;

	if (ipc.opt && ipc.opt->opt.srr) {
		if (!daddr) {
			err = -EINVAL;
			goto out_free;
		}
		faddr = ipc.opt->opt.faddr;
		connected = 0;
	}
	scope = ip_sendmsg_scope(inet, &ipc, msg);
	if (scope == RT_SCOPE_LINK)
		connected = 0;

	uc_index = READ_ONCE(inet->uc_index);
	if (ipv4_is_multicast(daddr)) {
		if (!ipc.oif || netif_index_is_l3_master(sock_net(sk), ipc.oif))
			ipc.oif = READ_ONCE(inet->mc_index);
		if (!saddr)
			saddr = READ_ONCE(inet->mc_addr);
		connected = 0;
	} else if (!ipc.oif) {
		ipc.oif = uc_index;
	} else if (ipv4_is_lbcast(daddr) && uc_index) {

		if (ipc.oif != uc_index &&
		    ipc.oif == l3mdev_master_ifindex_by_index(sock_net(sk),
							      uc_index)) {
			ipc.oif = uc_index;
		}
	}

	if (connected)
		rt = dst_rtable(sk_dst_check(sk, 0));

	if (!rt) {
		struct net *net = sock_net(sk);
		__u8 flow_flags = inet_sk_flowi_flags(sk);

		fl4 = &fl4_stack;

		flowi4_init_output(fl4, ipc.oif, ipc.sockc.mark,
				   ipc.tos & INET_DSCP_MASK, scope,
				   sk->sk_protocol, flow_flags, faddr, saddr,
				   dport, inet->inet_sport, sk->sk_uid);

		security_sk_classify_flow(sk, flowi4_to_flowi_common(fl4));
		rt = ip_route_output_flow(net, fl4, sk);
		if (IS_ERR(rt)) {
			err = PTR_ERR(rt);
			rt = NULL;
			if (err == -ENETUNREACH)
				IP_INC_STATS(net, IPSTATS_MIB_OUTNOROUTES);
			goto out;
		}

		err = -EACCES;
		if ((rt->rt_flags & RTCF_BROADCAST) &&
		    !sock_flag(sk, SOCK_BROADCAST))
			goto out;
		if (connected)
			sk_dst_set(sk, dst_clone(&rt->dst));
	}

	if (msg->msg_flags&MSG_CONFIRM)
		goto do_confirm;
back_from_confirm:

	saddr = fl4->saddr;
	if (!ipc.addr)
		daddr = ipc.addr = fl4->daddr;

	/* Lockless fast path for the non-corking case. */
	if (!corkreq) {
		struct inet_cork cork;

		skb = ip_make_skb(sk, fl4, getfrag, msg, ulen,
				  sizeof(struct udphdr), &ipc, &rt,
				  &cork, msg->msg_flags);
		err = PTR_ERR(skb);
		if (!IS_ERR_OR_NULL(skb))
			err = udp_send_skb(skb, fl4, &cork);
		goto out;
	}

	lock_sock(sk);
	if (unlikely(up->pending)) {
		/* The socket is already corked while preparing it. */
		/* ... which is an evident application bug. --ANK */
		release_sock(sk);

		net_dbg_ratelimited("socket already corked\n");
		err = -EINVAL;
		goto out;
	}
	/*
	 *	Now cork the socket to pend data.
	 */
	fl4 = &inet->cork.fl.u.ip4;
	fl4->daddr = daddr;
	fl4->saddr = saddr;
	fl4->fl4_dport = dport;
	fl4->fl4_sport = inet->inet_sport;
	WRITE_ONCE(up->pending, AF_INET);

do_append_data:
	up->len += ulen;
	err = ip_append_data(sk, fl4, getfrag, msg, ulen,
			     sizeof(struct udphdr), &ipc, &rt,
			     corkreq ? msg->msg_flags|MSG_MORE : msg->msg_flags);
	if (err)
		udp_flush_pending_frames(sk);
	else if (!corkreq)
		err = udp_push_pending_frames(sk);
	else if (unlikely(skb_queue_empty(&sk->sk_write_queue)))
		WRITE_ONCE(up->pending, 0);
	release_sock(sk);

out:
	ip_rt_put(rt);
out_free:
	if (free)
		kfree(ipc.opt);
	if (!err)
		return len;

	if (err == -ENOBUFS || test_bit(SOCK_NOSPACE, &sk->sk_socket->flags)) {
		UDP_INC_STATS(sock_net(sk),
			      UDP_MIB_SNDBUFERRORS, is_udplite);
	}
	return err;

do_confirm:
	if (msg->msg_flags & MSG_PROBE)
		dst_confirm_neigh(&rt->dst, &fl4->daddr);
	if (!(msg->msg_flags&MSG_PROBE) || len)
		goto back_from_confirm;
	err = 0;
	goto out;
}
EXPORT_SYMBOL(udp_sendmsg);
```

### 接收UDP数据包
```c
int udp_rcv(struct sk_buff *skb)
{
	return __udp4_lib_rcv(skb, dev_net(skb->dev)->ipv4.udp_table, IPPROTO_UDP);
}
```

```c
int __udp4_lib_rcv(struct sk_buff *skb, struct udp_table *udptable,
		   int proto)
{
	struct sock *sk = NULL;
	struct udphdr *uh;
	unsigned short ulen;
	struct rtable *rt = skb_rtable(skb);
	__be32 saddr, daddr;
	struct net *net = dev_net(skb->dev);
	bool refcounted;
	int drop_reason;

	drop_reason = SKB_DROP_REASON_NOT_SPECIFIED;

	if (!pskb_may_pull(skb, sizeof(struct udphdr)))
		goto drop;		/* No space for header. */

	/* 从SKBUF中取出UDP报头、长度、源地址和目标地址 */
	uh   = udp_hdr(skb);
	ulen = ntohs(uh->len);
	saddr = ip_hdr(skb)->saddr;
	daddr = ip_hdr(skb)->daddr;

	if (ulen > skb->len)
		goto short_packet;

	if (proto == IPPROTO_UDP) {
		/* UDP validates ulen. */
		if (ulen < sizeof(*uh) || pskb_trim_rcsum(skb, ulen))
			goto short_packet;
		uh = udp_hdr(skb);
	}

	if (udp4_csum_init(skb, uh, proto))
		goto csum_error;

	sk = inet_steal_sock(net, skb, sizeof(struct udphdr), saddr, uh->source, daddr, uh->dest,
			     &refcounted, udp_ehashfn);
	if (IS_ERR(sk))
		goto no_sk;

	if (sk) {
		struct dst_entry *dst = skb_dst(skb);
		int ret;

		if (unlikely(rcu_dereference(sk->sk_rx_dst) != dst))
			udp_sk_rx_dst_set(sk, dst);

		ret = udp_unicast_rcv_skb(sk, skb, uh);
		if (refcounted)
			sock_put(sk);
		return ret;
	}

	if (rt->rt_flags & (RTCF_BROADCAST|RTCF_MULTICAST))
		return __udp4_lib_mcast_deliver(net, skb, uh,
						saddr, daddr, udptable, proto);

	sk = __udp4_lib_lookup_skb(skb, uh->source, uh->dest, udptable);
	if (sk)
		return udp_unicast_rcv_skb(sk, skb, uh);
no_sk:
	if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb))
		goto drop;
	nf_reset_ct(skb);

	/* No socket. Drop packet silently, if checksum is wrong */
	if (udp_lib_checksum_complete(skb))
		goto csum_error;

	drop_reason = SKB_DROP_REASON_NO_SOCKET;
	__UDP_INC_STATS(net, UDP_MIB_NOPORTS, proto == IPPROTO_UDPLITE);
	icmp_send(skb, ICMP_DEST_UNREACH, ICMP_PORT_UNREACH, 0);

	sk_skb_reason_drop(sk, skb, drop_reason);
	return 0;

short_packet:
	drop_reason = SKB_DROP_REASON_PKT_TOO_SMALL;
	net_dbg_ratelimited("UDP%s: short packet: From %pI4:%u %d/%d to %pI4:%u\n",
			    proto == IPPROTO_UDPLITE ? "Lite" : "",
			    &saddr, ntohs(uh->source),
			    ulen, skb->len,
			    &daddr, ntohs(uh->dest));
	goto drop;

csum_error:

	drop_reason = SKB_DROP_REASON_UDP_CSUM;
	net_dbg_ratelimited("UDP%s: bad checksum. From %pI4:%u to %pI4:%u ulen %d\n",
			    proto == IPPROTO_UDPLITE ? "Lite" : "",
			    &saddr, ntohs(uh->source), &daddr, ntohs(uh->dest),
			    ulen);
	__UDP_INC_STATS(net, UDP_MIB_CSUMERRORS, proto == IPPROTO_UDPLITE);
drop:
	__UDP_INC_STATS(net, UDP_MIB_INERRORS, proto == IPPROTO_UDPLITE);
	sk_skb_reason_drop(sk, skb, drop_reason);
	return 0;
}
```
<img width="791" height="567" alt="image" src="https://github.com/user-attachments/assets/7d1820bd-9c4f-45bd-bb68-59f91c73578b" />
