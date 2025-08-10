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

```c

```
