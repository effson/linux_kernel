> include/linux/netdevice.h

```c
struct net_device {
    // ....
    unsigned long		mem_end; // 设备使用的物理内存区域的结束地址
    unsigned long		mem_start; // 设备使用的物理内存区域的起始地址
    unsigned long		base_addr;

    struct list_head	dev_list; //表示系统中所有 net_device 的链表节点。便于遍历所有设备
    struct list_head	napi_list; // 该设备上启用的所有 NAPI 实例（struct napi_struct）组成的链表
    struct list_head	unreg_list;
    struct list_head	close_list;
    struct list_head	ptype_all;
    /*含义：该设备上注册的全部 packet_type 的链表
    packet_type 是 Linux 中用来匹配协议类型的结构（如 ETH_P_ALL、ETH_P_IP 等）
    所有协议处理函数（如 ARP、IP）都挂在这里或 ptype_base[]*/

    struct {
	struct list_head upper;
	struct list_head lower;
    } adj_list;

    xdp_features_t		xdp_features;
    const struct xdp_metadata_ops *xdp_metadata_ops;
    const struct xsk_tx_metadata_ops *xsk_tx_metadata_ops;
    unsigned short		gflags;

    unsigned short		needed_tailroom;

    netdev_features_t	hw_features; // 设备硬件原生支持的功能
    netdev_features_t	wanted_features; //上层协议栈希望更改的功能
    netdev_features_t	vlan_features; // 设备对 VLAN 封装帧的 offload 能力
    netdev_features_t	hw_enc_features; // 设备对隧道封装（encapsulation，如 VXLAN、GENEVE）的 offload 能力
    netdev_features_t	mpls_features; // 设备对 MPLS（多协议标签交换）的报文是否支持某些 offload 特
    // ...
}
```
