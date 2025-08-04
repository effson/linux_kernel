#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#define BUFSIZE 8192

struct nl_req_s {
    struct nlmsghdr hdr;
    struct rtgenmsg gen;
};

void parse_link_info(struct nlmsghdr *nlh) {
    struct ifinfomsg *iface = NLMSG_DATA(nlh);
    struct rtattr *attr = IFLA_RTA(iface);
    int len = IFLA_PAYLOAD(nlh);

    char ifname[IF_NAMESIZE] = {0};
    unsigned char mac[6] = {0};

    for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
        switch (attr->rta_type) {
            case IFLA_IFNAME:
                strncpy(ifname, (char *)RTA_DATA(attr), IF_NAMESIZE - 1);
                break;
            case IFLA_ADDRESS:
                memcpy(mac, RTA_DATA(attr), 6);
                break;
        }
    }

    if (strlen(ifname) > 0) {
        printf("Interface: %s\n", ifname);
        printf("  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

int main() {
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_nl local = {
        .nl_family = AF_NETLINK,
        .nl_pid = getpid()
    };

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        return 1;
    }

    struct nl_req_s req;
    memset(&req, 0, sizeof(req));
    req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    req.hdr.nlmsg_type = RTM_GETLINK;
    req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.hdr.nlmsg_seq = 1;
    req.hdr.nlmsg_pid = getpid();
    req.gen.rtgen_family = AF_UNSPEC;

    struct sockaddr_nl kernel = {
        .nl_family = AF_NETLINK
    };

    struct iovec iov = {
        .iov_base = &req,
        .iov_len = req.hdr.nlmsg_len
    };

    struct msghdr msg = {
        .msg_name = &kernel,
        .msg_namelen = sizeof(kernel),
        .msg_iov = &iov,
        .msg_iovlen = 1
    };

    if (sendmsg(sock, &msg, 0) < 0) {
        perror("sendmsg");
        return 1;
    }

    char buf[BUFSIZE];
    int done = 0;
    while (!done) {
        struct iovec iov_resp = {
            .iov_base = buf,
            .iov_len = sizeof(buf)
        };
        struct msghdr msg_resp = {
            .msg_name = &local,
            .msg_namelen = sizeof(local),
            .msg_iov = &iov_resp,
            .msg_iovlen = 1
        };

        ssize_t len = recvmsg(sock, &msg_resp, 0);
        if (len < 0) {
            perror("recvmsg");
            break;
        }

        struct nlmsghdr *nlh;
        for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
            if (nlh->nlmsg_type == NLMSG_DONE) {
                done = 1;
                break;
            }
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                fprintf(stderr, "Received netlink error\n");
                done = 1;
                break;
            }
            if (nlh->nlmsg_type == RTM_NEWLINK) {
                parse_link_info(nlh);
            }
        }
    }

    close(sock);
    return 0;
}
