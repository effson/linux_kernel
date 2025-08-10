### 1.TCP头部
> 
```c
struct tcphdr {
	__be16	source; // 源端口号（16 位，大端）范围 1–65535
	__be16	dest; // 目的端口号（16 位，大端） 范围 1–65535
	__be32	seq; // 序列号（Sequence Number，32 位）
	__be32	ack_seq; // 确认号（Acknowledgment Number，32 位）
  //接收方期望的下一个字节序号（即已经收到的最后一个字节序号+1）
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u16	ae:1, // 在内核里是 ACK 扩展（ACKnowledgment Echo） 或类似功能位
		res1:3, // 保留位 保留给将来使用，通常为 0
		doff:4, // 数据偏移，单位是 4 字节（32 bit），表示 TCP 首部的长度
		fin:1, // 结束数据发送（连接半关闭）
		syn:1, // 建立连接时同步序列号
		rst:1, // 复位连接
		psh:1, // 推送功能（提示接收方立即将数据交给应用层）
		ack:1, // 确认号字段有效
		urg:1,
		ece:1, // ECN-Echo：用于显式拥塞通知（RFC 3168）
		cwr:1; // Congestion Window Reduced：发送方通知拥塞窗口已减少（RFC 3168）
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u16	doff:4,
		res1:3,
		ae:1,
		cwr:1,
		ece:1,
		urg:1,
		ack:1,
		psh:1,
		rst:1,
		syn:1,
		fin:1;
#else
#error	"Adjust your <asm/byteorder.h> defines"
#endif
	__be16	window; // 窗口大小（Window Size，16 位） 表示接收方的接收窗口大小（单位是字节）
	__sum16	check; // 校验和（Checksum，16 位）
	__be16	urg_ptr; // 紧急指针（Urgent Pointer，16 位） 当 URG=1 时有效，表示紧急数据在数据流中的位置
};
```
### 2. 初始化
> net/ipv4/af_inet.c
```c
	net_hotdata.tcp_protocol = (struct net_protocol) {
		.handler	=	tcp_v4_rcv,
		.err_handler	=	tcp_v4_err,
		.no_policy	=	1,
		.icmp_strict_tag_validation = 1,
	};
	if (inet_add_protocol(&net_hotdata.tcp_protocol, IPPROTO_TCP) < 0)
		pr_crit("%s: Cannot add TCP protocol\n", __func__);
```
### 3.TCP定时器
> net/ipv4/tcp_timer.c <br>

TCP 的定时器是保证 TCP 协议 可靠性、流量控制、拥塞控制、连接管理 正常运行的关键机制
- 重传定时器（Retransmission Timer / RTO Timer）,触发条件：发送的数据包在 RTO（Retransmission Timeout）内没收到 ACK
- 延迟 ACK 定时器（Delayed ACK Timer）,触发条件：收到数据但暂不立即发送 ACK，等待一段时间（如 40ms）看看能否与要发的数据一起捎带（piggyback）
- 保活定时器（Keepalive Timer）,触发条件：连接长时间无数据交互,作用：定期发送探测报文（keepalive probe）,检测对端是否存活（断网、崩溃等）,多次探测失败后断开连接
- 持续定时器（Persist Timer）,触发条件：接收端通告的窗口大小为 0（零窗口）,作用：发送 窗口探测报文（zero-window probe）,防止“死锁”——如果接收端恢复了窗口却没发通知，发送端会一直卡住
<br>
要使用TCP套接字，用户空间把必须创建SOCK_STREAM套接字，在内核里面由回调函数tcp_v4_init_socket()来处理，实际工作由tcp_init_sock()完成所有处理操作：

```c
void tcp_init_sock(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	int rto_min_us, rto_max_ms;

	/* 建立乱序报文红黑树根。接收端把到达但未按序的段暂存于此，等到达缺失段后再合并上交 */
	tp->out_of_order_queue = RB_ROOT;
	/* 建立重传队列红黑树根。发送端把已发送未确认的段挂在此队列，超时/丢包时用于重传与 SACK/RACK 判定 */
	sk->tcp_rtx_queue = RB_ROOT;
	/* 初始化 TCP 的关键定时器：重传（RTO）、延迟 ACK（delack）、保活（keepalive）等。定时器驱动可靠性与状态机推进 */
	tcp_init_xmit_timers(sk);
	INIT_LIST_HEAD(&tp->tsq_node);
	INIT_LIST_HEAD(&tp->tsorted_sent_queue);

	icsk->icsk_rto = TCP_TIMEOUT_INIT; // 设置初始 RTO（重传超时）。在尚无 RTT 样本时使用一个保守初值

	rto_max_ms = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_rto_max_ms);
	icsk->icsk_rto_max = msecs_to_jiffies(rto_max_ms);

	rto_min_us = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_rto_min_us);
	icsk->icsk_rto_min = usecs_to_jiffies(rto_min_us);
	icsk->icsk_delack_max = TCP_DELACK_MAX;
	tp->mdev_us = jiffies_to_usecs(TCP_TIMEOUT_INIT);
	minmax_reset(&tp->rtt_min, tcp_jiffies32, ~0U);

	/* 设置初始拥塞窗口 */
	tcp_snd_cwnd_set(tp, TCP_INIT_CWND);
	tp->app_limited = ~0U;
	tp->rate_app_limited = 1;
	tp->snd_ssthresh = TCP_INFINITE_SSTHRESH; // 慢启动阈值初值设为“无限大”
	tp->snd_cwnd_clamp = ~0; //拥塞窗口钳位上限设为最大

	tp->mss_cache = TCP_MSS_DEFAULT; /* MSS 缓存初值（占位值）真正有效的 MSS 会在路由/PMTU、
										选项（如 TS/WS）变化时由 tcp_sync_mss() 更新 */
	/* 可容忍乱序步数，影响 SACK/RACK 对乱序与丢包的判定阈值 */
	tp->reordering = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_reordering);
	tcp_assign_congestion_control(sk); // 绑定当前套接字的拥塞控制算法

	tp->tsoffset = 0; // TCP 时间戳偏移初值
	tp->rack.reo_wnd_steps = 1; // RACK 的乱序窗口步进设置，影响按时间/乱序程度判断丢包的敏感度

	sk->sk_write_space = sk_stream_write_space;
	sock_set_flag(sk, SOCK_USE_WRITE_QUEUE);

	icsk->icsk_sync_mss = tcp_sync_mss;  // 设置MSS 同步函数指针

	WRITE_ONCE(sk->sk_sndbuf, READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_wmem[1]));
	WRITE_ONCE(sk->sk_rcvbuf, READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_rmem[1]));
	tcp_scaling_ratio_init(sk);

	set_bit(SOCK_SUPPORT_ZC, &sk->sk_socket->flags); // 声明此 socket 支持零拷贝（如 MSG_ZEROCOPY）
	sk_sockets_allocated_inc(sk);
	xa_init_flags(&sk->sk_user_frags, XA_FLAGS_ALLOC1);
}
EXPORT_IPV6_MOD(tcp_init_sock);
```

### 4.接收网络层的数据包
> net/ipv4/tcp_ipv4.c

```c
int tcp_v4_rcv(struct sk_buff *skb)
{
	// ...
	/* 不是发给本机就丢弃 */
	if (skb->pkt_type != PACKET_HOST)
		goto discard_it;

	/* 包长是否大于TCP头的长度 */
	if (!pskb_may_pull(skb, sizeof(struct tcphdr)))
		goto discard_it;
	/* 获取TCP首部 */
	th = (const struct tcphdr *)skb->data;

	// 检查TCP首部 doff、checksum
lookup: // 查找匹配的socket，找不到则丢弃
	sk = __inet_lookup_skb(net->ipv4.tcp_death_row.hashinfo,
			       skb, __tcp_hdrlen(th), th->source,
			       th->dest, sdif, &refcounted);
	// ...
process:
	/* 检查IPSEC规则 */
	if (!xfrm4_policy_check(sk, XFRM_POLICY_IN, skb)) {
		drop_reason = SKB_DROP_REASON_XFRM_POLICY;
		goto discard_and_relse;
	}
	// ...
	// 检查BPF规则
	if (tcp_filter(sk, skb)) {
		drop_reason = SKB_DROP_REASON_SOCKET_FILTER;
		goto discard_and_relse;
	}
	// ...
	// 检查socket是否被用户态锁定，为真则进入后备处理队列
	if (!sock_owned_by_user(sk)) {
		ret = tcp_v4_do_rcv(sk, skb);
	} else {
		if (tcp_add_backlog(sk, skb, &drop_reason))
			goto discard_and_relse;
	}
	// ...
}
```
### 5.接收网络层的数据包
> net/ipv4/tcp.c
```c
int tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
	int ret;

	lock_sock(sk);
	ret = tcp_sendmsg_locked(sk, msg, size);
	release_sock(sk);

	return ret;
}
```
```c
int tcp_sendmsg_locked(struct sock *sk, struct msghdr *msg, size_t size)
{
	// ...
	// 如果sendmsg是阻塞操作，获取阻塞时间
	timeo = sock_sndtimeo(sk, flags & MSG_DONTWAIT);

	tcp_rate_check_app_limited(sk);  /* is sending application-limited? */

	/* 如果不在TCPF_ESTABLISHED 或 TCPF_CLOSE_WAIT，等待连接完成.  */
	if (((1 << sk->sk_state) & ~(TCPF_ESTABLISHED | TCPF_CLOSE_WAIT)) &&
	    !tcp_passive_fastopen(sk)) {
		err = sk_stream_wait_connect(sk, &timeo);
		if (err != 0)
			goto do_error;
	}

	if (unlikely(tp->repair)) {
		if (tp->repair_queue == TCP_RECV_QUEUE) {
			copied = tcp_send_rcvq(sk, msg, size);
			goto out_nopush;
		}

		err = -EINVAL;
		if (tp->repair_queue == TCP_NO_QUEUE)
			goto out_err;

		/* 'common' sending to sendq */
	}

	sockc = (struct sockcm_cookie) { .tsflags = READ_ONCE(sk->sk_tsflags)};
	if (msg->msg_controllen) {
		err = sock_cmsg_send(sk, msg, &sockc);
		if (unlikely(err)) {
			err = -EINVAL;
			goto out_err;
		}
	}

	/* This should be in poll */
	sk_clear_bit(SOCKWQ_ASYNC_NOSPACE, sk);

	/* Ok commence sending. */
	copied = 0;

restart:
	mss_now = tcp_send_mss(sk, &size_goal, flags);

	err = -EPIPE;
	if (sk->sk_err || (sk->sk_shutdown & SEND_SHUTDOWN))
		goto do_error;

	while (msg_data_left(msg)) {
		ssize_t copy = 0;

		skb = tcp_write_queue_tail(sk);
		if (skb)
			copy = size_goal - skb->len;

		if (copy <= 0 || !tcp_skb_can_collapse_to(skb)) {
			bool first_skb;

new_segment:
			if (!sk_stream_memory_free(sk))
				goto wait_for_space;

			if (unlikely(process_backlog >= 16)) {
				process_backlog = 0;
				if (sk_flush_backlog(sk))
					goto restart;
			}
			first_skb = tcp_rtx_and_write_queues_empty(sk);
			skb = tcp_stream_alloc_skb(sk, sk->sk_allocation,
						   first_skb);
			if (!skb)
				goto wait_for_space;

			process_backlog++;

#ifdef CONFIG_SKB_DECRYPTED
			skb->decrypted = !!(flags & MSG_SENDPAGE_DECRYPTED);
#endif
			tcp_skb_entail(sk, skb);
			copy = size_goal;

			/* All packets are restored as if they have
			 * already been sent. skb_mstamp_ns isn't set to
			 * avoid wrong rtt estimation.
			 */
			if (tp->repair)
				TCP_SKB_CB(skb)->sacked |= TCPCB_REPAIRED;
		}

		/* Try to append data to the end of skb. */
		if (copy > msg_data_left(msg))
			copy = msg_data_left(msg);

		if (zc == 0) {
			bool merge = true;
			int i = skb_shinfo(skb)->nr_frags;
			struct page_frag *pfrag = sk_page_frag(sk);

			if (!sk_page_frag_refill(sk, pfrag))
				goto wait_for_space;

			if (!skb_can_coalesce(skb, i, pfrag->page,
					      pfrag->offset)) {
				if (i >= READ_ONCE(net_hotdata.sysctl_max_skb_frags)) {
					tcp_mark_push(tp, skb);
					goto new_segment;
				}
				merge = false;
			}

			copy = min_t(int, copy, pfrag->size - pfrag->offset);

			if (unlikely(skb_zcopy_pure(skb) || skb_zcopy_managed(skb))) {
				if (tcp_downgrade_zcopy_pure(sk, skb))
					goto wait_for_space;
				skb_zcopy_downgrade_managed(skb);
			}

			copy = tcp_wmem_schedule(sk, copy);
			if (!copy)
				goto wait_for_space;

			err = skb_copy_to_page_nocache(sk, &msg->msg_iter, skb,
						       pfrag->page,
						       pfrag->offset,
						       copy);
			if (err)
				goto do_error;

			/* Update the skb. */
			if (merge) {
				skb_frag_size_add(&skb_shinfo(skb)->frags[i - 1], copy);
			} else {
				skb_fill_page_desc(skb, i, pfrag->page,
						   pfrag->offset, copy);
				page_ref_inc(pfrag->page);
			}
			pfrag->offset += copy;
		} else if (zc == MSG_ZEROCOPY)  {
			/* First append to a fragless skb builds initial
			 * pure zerocopy skb
			 */
			if (!skb->len)
				skb_shinfo(skb)->flags |= SKBFL_PURE_ZEROCOPY;

			if (!skb_zcopy_pure(skb)) {
				copy = tcp_wmem_schedule(sk, copy);
				if (!copy)
					goto wait_for_space;
			}

			err = skb_zerocopy_iter_stream(sk, skb, msg, copy, uarg);
			if (err == -EMSGSIZE || err == -EEXIST) {
				tcp_mark_push(tp, skb);
				goto new_segment;
			}
			if (err < 0)
				goto do_error;
			copy = err;
		} else if (zc == MSG_SPLICE_PAGES) {
			/* Splice in data if we can; copy if we can't. */
			if (tcp_downgrade_zcopy_pure(sk, skb))
				goto wait_for_space;
			copy = tcp_wmem_schedule(sk, copy);
			if (!copy)
				goto wait_for_space;

			err = skb_splice_from_iter(skb, &msg->msg_iter, copy,
						   sk->sk_allocation);
			if (err < 0) {
				if (err == -EMSGSIZE) {
					tcp_mark_push(tp, skb);
					goto new_segment;
				}
				goto do_error;
			}
			copy = err;

			if (!(flags & MSG_NO_SHARED_FRAGS))
				skb_shinfo(skb)->flags |= SKBFL_SHARED_FRAG;

			sk_wmem_queued_add(sk, copy);
			sk_mem_charge(sk, copy);
		}

		if (!copied)
			TCP_SKB_CB(skb)->tcp_flags &= ~TCPHDR_PSH;

		WRITE_ONCE(tp->write_seq, tp->write_seq + copy);
		TCP_SKB_CB(skb)->end_seq += copy;
		tcp_skb_pcount_set(skb, 0);

		copied += copy;
		if (!msg_data_left(msg)) {
			if (unlikely(flags & MSG_EOR))
				TCP_SKB_CB(skb)->eor = 1;
			goto out;
		}

		if (skb->len < size_goal || (flags & MSG_OOB) || unlikely(tp->repair))
			continue;

		if (forced_push(tp)) {
			tcp_mark_push(tp, skb);
			__tcp_push_pending_frames(sk, mss_now, TCP_NAGLE_PUSH);
		} else if (skb == tcp_send_head(sk))
			tcp_push_one(sk, mss_now);
		continue;

wait_for_space:
		set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
		tcp_remove_empty_skb(sk);
		if (copied)
			tcp_push(sk, flags & ~MSG_MORE, mss_now,
				 TCP_NAGLE_PUSH, size_goal);

		err = sk_stream_wait_memory(sk, &timeo);
		if (err != 0)
			goto do_error;

		mss_now = tcp_send_mss(sk, &size_goal, flags);
	}

out:
	if (copied) {
		tcp_tx_timestamp(sk, &sockc);
		tcp_push(sk, flags, mss_now, tp->nonagle, size_goal);
	}
out_nopush:
	/* msg->msg_ubuf is pinned by the caller so we don't take extra refs */
	if (uarg && !msg->msg_ubuf)
		net_zcopy_put(uarg);
	return copied + copied_syn;

do_error:
	tcp_remove_empty_skb(sk);

	if (copied + copied_syn)
		goto out;
out_err:
	/* msg->msg_ubuf is pinned by the caller so we don't take extra refs */
	if (uarg && !msg->msg_ubuf)
		net_zcopy_put_abort(uarg, true);
	err = sk_stream_error(sk, flags, err);
	/* make sure we wake any epoll edge trigger waiter */
	if (unlikely(tcp_rtx_and_write_queues_empty(sk) && err == -EAGAIN)) {
		sk->sk_write_space(sk);
		tcp_chrono_stop(sk, TCP_CHRONO_SNDBUF_LIMITED);
	}
	return err;
}
EXPORT_SYMBOL_GPL(tcp_sendmsg_locked);
```
