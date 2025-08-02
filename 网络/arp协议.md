> include/uapi/linux/if_arp.h
```c
struct arphdr {
	__be16		ar_hrd;		/* 硬件类型	*/
	__be16		ar_pro;		/* 协议类型	*/
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
