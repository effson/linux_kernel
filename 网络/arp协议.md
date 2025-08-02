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
