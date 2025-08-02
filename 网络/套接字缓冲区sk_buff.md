### 2.5 套接字缓冲区
sk_buff（socket buffer）是 Linux 网络协议栈中最核心的数据结构之一，在内核各层之间传递网络数据包，是 Linux 网络栈中所有数据包的封装容器
#### 2.5.1 内核结构体
> include/linux/skbuff.h
```c
struct sk_buff {
    union {
	struct {
	    /* These two members must be first to match sk_buff_head. */
 	    struct sk_buff  *next;   //sk_buff是双向链表
	    struct sk_buff  *prev;

	    union {
		struct net_device	*dev;
		/* 指向网络设备的指针，如：eth0, lo, br0，收到此报文的设备
		 * 对于接收路径：表示 收到该 skb 的设备
		 * 对于发送路径：表示 即将发送到哪个设备
		 */
		unsigned long		dev_scratch; // “临时用途”字段空间，供某些协议或路径复用
		// 如:UDP接收路径中，skb不需要dev，可以把这块内存用来存储别的内容（节省空间）
	    };
	};
	struct rb_node		rbnode; /* 红黑树节点某些场景中skb不走链表，而是按顺序（时间戳、序号）放入红黑树，例如：
	netem（网络仿真）、TCP out-of-order queue（乱序缓存）、IPv4 分片重组（ip_defrag） */

	struct list_head	list; // 用于将 skb 插入到 list_head 链表中
	struct llist_node	ll_node; // Lockless list（无锁链表） 的节点，提供原子插入能力
    };

    struct sock  *sk; /* 指向该skb所属的 socket（如TCP/UDP连接），仅在本机发出的报文中有用
			协议栈需要知道哪个 socket 收到这个数据，TCP 需要通过sk获取发送窗口、序列号等连接状态*/

    union {
	ktime_t		tstamp; // 表示 skb 的接收或发送时间戳，用于 TCP RTT 估计、网络延迟分析、BPF 等功能
	u64		skb_mstamp_ns; /* earliest departure time 精确的时间戳（单位为纳秒）*/
    };

    char    cb[48] __aligned(8); // 通用用途的私有缓冲区，供协议栈各层在处理 sk_buff 时临时存放控制信息使用

    union {
	struct {
	    unsigned long	_skb_refdst;
	    void		(*destructor)(struct sk_buff *skb);
        };

        struct list_head	tcp_tsorted_anchor;
#ifdef CONFIG_NET_SOCK_MSG
	unsigned long	_sk_redir;
#endif
    };

#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
    unsigned long	_nfct;
#endif
    unsigned int	len,  // skb 中当前所有数据的总长度 len = skb->tail - skb->data + skb->data_len;
			data_len;  // skb 中没有线性映射（non-linear）的数据长度
    __u16		mac_len,// 含义：链路层（MAC 层）头部长度
			hdr_len;// 协议栈构造或处理该 skb 时，所有协议头部的总长度

    __u16		queue_mapping;

/* if you move cloned around you also must adapt those constants */
#ifdef __BIG_ENDIAN_BITFIELD
#define CLONED_MASK	(1 << 7)
#else
#define CLONED_MASK	1
#endif
#define CLONED_OFFSET	offsetof(struct sk_buff, __cloned_offset)

	/* private: */
	__u8		__cloned_offset[0];
	/* public: */
    __u8		cloned:1,
			nohdr:1,
			fclone:2,
			peeked:1,
			head_frag:1,
			pfmemalloc:1,
			pp_recycle:1; /* page_pool recycle indicator */
#ifdef CONFIG_SKB_EXTENSIONS
    __u8		active_extensions;
#endif

    struct_group(headers,

	/* private: */
    __u8		__pkt_type_offset[0];
	/* public: */
    __u8		pkt_type:3; /* see PKT_TYPE_MAX */
    __u8		ignore_df:1;
    __u8		dst_pending_confirm:1;
    __u8		ip_summed:2;
    __u8		ooo_okay:1;

	/* private: */
    __u8		__mono_tc_offset[0];
	/* public: */
    __u8		tstamp_type:2;	/* See skb_tstamp_type */
#ifdef CONFIG_NET_XGRESS
    __u8		tc_at_ingress:1;	/* See TC_AT_INGRESS_MASK */
    __u8		tc_skip_classify:1;
#endif
    __u8		remcsum_offload:1;
    __u8		csum_complete_sw:1;
    __u8		csum_level:2;
    __u8		inner_protocol_type:1;

    __u8		l4_hash:1;
    __u8		sw_hash:1;
#ifdef CONFIG_WIRELESS
    __u8		wifi_acked_valid:1;
    __u8		wifi_acked:1;
#endif
    __u8		no_fcs:1;
	/* Indicates the inner headers are valid in the skbuff. */
    __u8		encapsulation:1;
    __u8		encap_hdr_csum:1;
    __u8		csum_valid:1;
#ifdef CONFIG_IPV6_NDISC_NODETYPE
    __u8		ndisc_nodetype:2;
#endif

#if IS_ENABLED(CONFIG_IP_VS)
    __u8		ipvs_property:1;
#endif
#if IS_ENABLED(CONFIG_NETFILTER_XT_TARGET_TRACE) || IS_ENABLED(CONFIG_NF_TABLES)
    __u8		nf_trace:1;
#endif
#ifdef CONFIG_NET_SWITCHDEV
    __u8		offload_fwd_mark:1;
    __u8		offload_l3_fwd_mark:1;
#endif
    __u8		redirected:1;
#ifdef CONFIG_NET_REDIRECT
    __u8		from_ingress:1;
#endif
#ifdef CONFIG_NETFILTER_SKIP_EGRESS
    __u8		nf_skip_egress:1;
#endif
#ifdef CONFIG_SKB_DECRYPTED
    __u8		decrypted:1;
#endif
    __u8		slow_gro:1;
#if IS_ENABLED(CONFIG_IP_SCTP)
    __u8		csum_not_inet:1;
#endif
    __u8		unreadable:1;
#if defined(CONFIG_NET_SCHED) || defined(CONFIG_NET_XGRESS)
    __u16		tc_index;	/* traffic control index */
#endif

    u16			alloc_cpu;

    union {
	__wsum		csum; // 校验和
	struct {
	    __u16	csum_start;
	    __u16	csum_offset;
        };
    };
    __u32			priority;
    int			skb_iif;
    __u32			hash;
    union {
	u32		vlan_all;
	struct {
	    __be16	vlan_proto;
	    __u16	vlan_tci;
	};
    };
#if defined(CONFIG_NET_RX_BUSY_POLL) || defined(CONFIG_XPS)
    union {
	unsigned int	napi_id;
	unsigned int	sender_cpu;
    };
#endif
#ifdef CONFIG_NETWORK_SECMARK
    __u32		secmark;
#endif

    union {
	__u32		mark;
	__u32		reserved_tailroom;
    };

    union {
	__be16		inner_protocol;
	__u8		inner_ipproto;
    };

    __u16		inner_transport_header; // 封装的原始数据包的协议信息
    __u16		inner_network_header;
    __u16		inner_mac_header;

    __be16		protocol;  //最外层的协议类型（以太网 ethertype）
    __u16		transport_header; //相对于 skb->head 的偏移量
    __u16		network_header;
    __u16		mac_header;

#ifdef CONFIG_KCOV
    u64			kcov_handle;
#endif

    );
    /*描述数据缓冲区（data buffer）的位置与布局
          head                  data               tail            end
           ↓                     ↓                  ↓              ↓
+----------+---------------------+------------------+--------------+
| reserved | protocol headers    |     payload      |  unused      |
+----------+---------------------+------------------+--------------+
*/
    sk_buff_data_t	tail; // 当前数据末尾（写指针）
    sk_buff_data_t	end; // 缓冲区末尾（head + end）
    unsigned char	*head, //缓冲区起始地址
			*data; // 当前数据起始地址（可变）
    unsigned int	truesize; // 表示 这个 skb 及其相关资源实际占用了多少字节的内存
    refcount_t		users;

#ifdef CONFIG_SKB_EXTENSIONS
	/* only usable after checking ->active_extensions != 0 */
    struct skb_ext	*extensions;
#endif
};
```
<img width="517" height="363" alt="image" src="https://github.com/user-attachments/assets/2ec96edc-6db1-4d1b-aa3a-8602327c8dd5" />


