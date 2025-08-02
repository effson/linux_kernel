/*
 * Author: Jeff
 * Date: 2025-07-30
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <net/sock.h>
#include <linux/netlink.h>

#define NETLINK_TEST 30
#define MSG_LEN 125
#define USER_PORT 100

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff");
MODULE_DESCRIPTION("Netlink Test Module");

struct sock *nl_sk = NULL;
extern struct net init_net;

static int send_msg_to_user(const char *pbuf, int len)
{
    struct sk_buff *nl_skb;
    struct nlmsghdr *nlh;

    int ret;
    nl_skb = nlmsg_new(len, GFP_ATOMIC);
    if (!nl_skb) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return -ENOMEM;
    }

    // #define RTM_GETNEIGH    30  /* 查询邻居项（如 ARP 缓存、NDP 缓存） */
    nlh = nlmsg_put(nl_skb, 0, 0, NETLINK_TEST, len, 0);
    if (!nlh) {
        printk(KERN_ERR "Failed to put nlmsg\n");
        nlmsg_free(nl_skb);
        return -ENOMEM;
    }

    memcpy(nlmsg_data(nlh), pbuf, len);

    ret = netlink_unicast(nl_sk, nl_skb, USER_PORT, MSG_DONTWAIT);
    return ret;
}

static void  netlink_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    char *umsg = NULL;
    char *kmsg = "Hello from kernel module!";

    if (skb->len >= nlmsg_total_size(0)) {
        nlh = nlmsg_hdr(skb);
        umsg = NLMSG_DATA(nlh);
        if (umsg) {
            printk(KERN_INFO "Received message from user: %s\n", kmsg);
            send_msg_to_user(kmsg, strlen(kmsg));
        }
    }
}

static struct netlink_kernel_cfg cfg = {
    .input = netlink_recv_msg,
};

static int test_netlink_init(void)
{
    nl_sk = (struct sock *)netlink_kernel_create(&init_net, NETLINK_TEST, &cfg);
    if (!nl_sk) {
        printk(KERN_ERR "Failed to create netlink socket\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "Netlink socket created successfully\n");
    return 0;
}

static void test_netlink_exit(void)
{
    if (nl_sk) {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
        printk(KERN_INFO "Netlink socket released\n");
    }
}

module_init(test_netlink_init);
module_exit(test_netlink_exit);
