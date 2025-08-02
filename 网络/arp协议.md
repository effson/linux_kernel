### 1. 内核结构体
> include/uapi/linux/if_arp.h
```c
struct arphdr {
	__be16		ar_hrd;		/* 硬件类型	*/
	__be16		ar_pro;		/* 协议类型 ipv4为0x80	*/
	unsigned char	ar_hln;		/* 硬件地址长度	*/
	unsigned char	ar_pln;		/* 协议地址长度	*/
	__be16		ar_op;		/* ARP 操作类型	*/

#if 0
	 /*
	  *	 Ethernet looks like this : This bit is variable sized however...
	  */
	unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
	unsigned char		ar_sip[4];		/* sender IP address		*/
	unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
	unsigned char		ar_tip[4];		/* target IP address		*/
#endif
};
```

ar_pro:
```
#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_IPV6	0x86DD		/* IPv6 over bluebook		*/
```
ar_op:
```
ARPOP_REQUEST	1	ARP 请求（Who has IP X? Tell me your MAC）
ARPOP_REPLY	2	ARP 应答（IP X is at MAC Y）
ARPOP_RREQUEST	3	RARP 请求（少见）
ARPOP_RREPLY	4	RARP 应答
ARPOP_InREQUEST	8	Inverse ARP 请求（Frame Relay 使用）
ARPOP_InREPLY	9	Inverse ARP 应答
ARPOP_NAK	10	ARP NAK（不常用，部分协议扩展使用）
```
### 2.ARP请求帧格式
<img width="682" height="63" alt="image" src="https://github.com/user-attachments/assets/a4b187bf-c8e7-4a3a-8bc7-d3dd28d383d2" />

### 3.ARP请求的发送
> net/ipv4/ip_output.c
```c
// 在 IP 层完成包的准备工作后，将 IP 数据包传递给链路层进行发送（封装 MAC 层头并交给网卡发送）
static int ip_finish_output2(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	struct dst_entry *dst = skb_dst(skb);
	struct rtable *rt = dst_rtable(dst);
	struct net_device *dev = dst->dev;
	unsigned int hh_len = LL_RESERVED_SPACE(dev);
	struct neighbour *neigh;
	bool is_v6gw = false;

	if (rt->rt_type == RTN_MULTICAST) {
		IP_UPD_PO_STATS(net, IPSTATS_MIB_OUTMCAST, skb->len);
	} else if (rt->rt_type == RTN_BROADCAST)
		IP_UPD_PO_STATS(net, IPSTATS_MIB_OUTBCAST, skb->len);

	/* OUTOCTETS should be counted after fragment */
	IP_UPD_PO_STATS(net, IPSTATS_MIB_OUT, skb->len);

	if (unlikely(skb_headroom(skb) < hh_len && dev->header_ops)) {
		skb = skb_expand_head(skb, hh_len);
		if (!skb)
			return -ENOMEM;
	}

	if (lwtunnel_xmit_redirect(dst->lwtstate)) {
		int res = lwtunnel_xmit(skb);

		if (res != LWTUNNEL_XMIT_CONTINUE)
			return res;
	}

	rcu_read_lock();
	neigh = ip_neigh_for_gw(rt, skb, &is_v6gw);
	if (!IS_ERR(neigh)) {
		int res;

		sock_confirm_neigh(skb, neigh);
		/* if crossing protocols, can not use the cached header */
		res = neigh_output(neigh, skb, is_v6gw);  // 调用 struct neighbour->struct neigh_ops -> output函数
		rcu_read_unlock();
		return res;
	}
	rcu_read_unlock();

	net_dbg_ratelimited("%s: No header cache and no neighbour!\n",
			    __func__);
	kfree_skb_reason(skb, SKB_DROP_REASON_NEIGH_CREATEFAIL);
	return PTR_ERR(neigh);
}
```
