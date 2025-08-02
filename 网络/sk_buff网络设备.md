### 1. 网络设备内核结构体

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
    unsigned short	gflags;

    unsigned short	needed_tailroom;

    netdev_features_t	hw_features; // 设备硬件原生支持的功能
    netdev_features_t	wanted_features; //上层协议栈希望更改的功能
    netdev_features_t	vlan_features; // 设备对 VLAN 封装帧的 offload 能力
    netdev_features_t	hw_enc_features; // 设备对隧道封装（encapsulation，如 VXLAN、GENEVE）的 offload 能力
    netdev_features_t	mpls_features; // 设备对 MPLS（多协议标签交换）的报文是否支持某些 offload 特
    // ...
    unsigned int	mtu; // 当前网络接口的最大传输单元（MTU, Maximum Transmission Unit） 
    nsigned int		min_mtu; // 设备允许设置的最小 MTU
    unsigned int	max_mtu; // 备允许设置的最大 MTU
    unsigned short	type; // 设备类型（设备的链路层协议类型）
    unsigned char	min_header_len; // 设备发送数据包时所需的最小链路层头部长度
    unsigned char	name_assign_type; // 记录该设备的名称（如 eth0、ens33）是如何分配的
}
```
### 2.网络设备的注册和注销

> net/core/dev.c

```c
int register_netdevice(struct net_device *dev)
{
	int ret;
	struct net *net = dev_net(dev);

	BUILD_BUG_ON(sizeof(netdev_features_t) * BITS_PER_BYTE <
		     NETDEV_FEATURE_COUNT);
	BUG_ON(dev_boot_phase);
	ASSERT_RTNL();

	might_sleep();

	/* When net_device's are persistent, this will be fatal. */
	BUG_ON(dev->reg_state != NETREG_UNINITIALIZED);
	BUG_ON(!net);

	ret = ethtool_check_ops(dev->ethtool_ops);
	if (ret)
		return ret;

	/* rss ctx ID 0 is reserved for the default context, start from 1 */
	xa_init_flags(&dev->ethtool->rss_ctx, XA_FLAGS_ALLOC1);
	mutex_init(&dev->ethtool->rss_lock);

	spin_lock_init(&dev->addr_list_lock);
	netdev_set_addr_lockdep_class(dev);

	ret = dev_get_valid_name(net, dev, dev->name);
	if (ret < 0)
		goto out;

	ret = -ENOMEM;
	dev->name_node = netdev_name_node_head_alloc(dev);
	if (!dev->name_node)
		goto out;

	/* Init, if this function is available */
	if (dev->netdev_ops->ndo_init) {
		ret = dev->netdev_ops->ndo_init(dev);
		if (ret) {
			if (ret > 0)
				ret = -EIO;
			goto err_free_name;
		}
	}

	if (((dev->hw_features | dev->features) &
	     NETIF_F_HW_VLAN_CTAG_FILTER) &&
	    (!dev->netdev_ops->ndo_vlan_rx_add_vid ||
	     !dev->netdev_ops->ndo_vlan_rx_kill_vid)) {
		netdev_WARN(dev, "Buggy VLAN acceleration in driver!\n");
		ret = -EINVAL;
		goto err_uninit;
	}

	ret = netdev_do_alloc_pcpu_stats(dev);
	if (ret)
		goto err_uninit;

	ret = dev_index_reserve(net, dev->ifindex);
	if (ret < 0)
		goto err_free_pcpu;
	dev->ifindex = ret;

	/* Transfer changeable features to wanted_features and enable
	 * software offloads (GSO and GRO).
	 */
	dev->hw_features |= (NETIF_F_SOFT_FEATURES | NETIF_F_SOFT_FEATURES_OFF);
	dev->features |= NETIF_F_SOFT_FEATURES;

	if (dev->udp_tunnel_nic_info) {
		dev->features |= NETIF_F_RX_UDP_TUNNEL_PORT;
		dev->hw_features |= NETIF_F_RX_UDP_TUNNEL_PORT;
	}

	dev->wanted_features = dev->features & dev->hw_features;

	if (!(dev->flags & IFF_LOOPBACK))
		dev->hw_features |= NETIF_F_NOCACHE_COPY;

	/* If IPv4 TCP segmentation offload is supported we should also
	 * allow the device to enable segmenting the frame with the option
	 * of ignoring a static IP ID value.  This doesn't enable the
	 * feature itself but allows the user to enable it later.
	 */
	if (dev->hw_features & NETIF_F_TSO)
		dev->hw_features |= NETIF_F_TSO_MANGLEID;
	if (dev->vlan_features & NETIF_F_TSO)
		dev->vlan_features |= NETIF_F_TSO_MANGLEID;
	if (dev->mpls_features & NETIF_F_TSO)
		dev->mpls_features |= NETIF_F_TSO_MANGLEID;
	if (dev->hw_enc_features & NETIF_F_TSO)
		dev->hw_enc_features |= NETIF_F_TSO_MANGLEID;

	/* Make NETIF_F_HIGHDMA inheritable to VLAN devices.
	 */
	dev->vlan_features |= NETIF_F_HIGHDMA;

	/* Make NETIF_F_SG inheritable to tunnel devices.
	 */
	dev->hw_enc_features |= NETIF_F_SG | NETIF_F_GSO_PARTIAL;

	/* Make NETIF_F_SG inheritable to MPLS.
	 */
	dev->mpls_features |= NETIF_F_SG;

	ret = call_netdevice_notifiers(NETDEV_POST_INIT, dev);
	ret = notifier_to_errno(ret);
	if (ret)
		goto err_ifindex_release;

	ret = netdev_register_kobject(dev);

	netdev_lock(dev);
	WRITE_ONCE(dev->reg_state, ret ? NETREG_UNREGISTERED : NETREG_REGISTERED);
	netdev_unlock(dev);

	if (ret)
		goto err_uninit_notify;

	netdev_lock_ops(dev);
	__netdev_update_features(dev);
	netdev_unlock_ops(dev);

	/*
	 *	Default initial state at registry is that the
	 *	device is present.
	 */

	set_bit(__LINK_STATE_PRESENT, &dev->state);

	linkwatch_init_dev(dev);

	dev_init_scheduler(dev);

	netdev_hold(dev, &dev->dev_registered_tracker, GFP_KERNEL);
	list_netdevice(dev);

	add_device_randomness(dev->dev_addr, dev->addr_len);

	/* If the device has permanent device address, driver should
	 * set dev_addr and also addr_assign_type should be set to
	 * NET_ADDR_PERM (default value).
	 */
	if (dev->addr_assign_type == NET_ADDR_PERM)
		memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	/* Notify protocols, that a new device appeared. */
	ret = call_netdevice_notifiers(NETDEV_REGISTER, dev);
	ret = notifier_to_errno(ret);
	if (ret) {
		/* Expect explicit free_netdev() on failure */
		dev->needs_free_netdev = false;
		unregister_netdevice_queue(dev, NULL);
		goto out;
	}
	/*
	 *	Prevent userspace races by waiting until the network
	 *	device is fully setup before sending notifications.
	 */
	if (!dev->rtnl_link_ops ||
	    dev->rtnl_link_state == RTNL_LINK_INITIALIZED)
		rtmsg_ifinfo(RTM_NEWLINK, dev, ~0U, GFP_KERNEL, 0, NULL);

out:
	return ret;

err_uninit_notify:
	call_netdevice_notifiers(NETDEV_PRE_UNINIT, dev);
err_ifindex_release:
	dev_index_release(net, dev->ifindex);
err_free_pcpu:
	netdev_do_free_pcpu_stats(dev);
err_uninit:
	if (dev->netdev_ops->ndo_uninit)
		dev->netdev_ops->ndo_uninit(dev);
	if (dev->priv_destructor)
		dev->priv_destructor(dev);
err_free_name:
	netdev_name_node_free(dev->name_node);
	goto out;
}
```

> include/linux/netdevice.h
```c
static inline void unregister_netdevice(struct net_device *dev)
{
	unregister_netdevice_queue(dev, NULL);
}

void unregister_netdevice_queue(struct net_device *dev, struct list_head *head)
{
	ASSERT_RTNL();

	if (head) {
		list_move_tail(&dev->unreg_list, head);
	} else {
		LIST_HEAD(single);

		list_add(&dev->unreg_list, &single);
		unregister_netdevice_many(&single);
	}
}
```
