#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <net/if.h>

#define BUFSIZE 8192

// 显示接口名和 MAC 地址（RTM_NEWLINK）
void handle_link(struct nlmsghdr *nlh) {
    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    struct rtattr *attr = IFLA_RTA(ifi);
    int len = IFLA_PAYLOAD(nlh);

    char ifname[IF_NAMESIZE] = "";
    unsigned char mac[6] = {0};

    for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
        if (attr->rta_type == IFLA_IFNAME) {
            strncpy(ifname, RTA_DATA(attr), IF_NAMESIZE - 1);
        } else if (attr->rta_type == IFLA_ADDRESS && RTA_PAYLOAD(attr) == 6) {
            memcpy(mac, RTA_DATA(attr), 6);
        }
    }

    printf("Interface: %s\n", ifname);
    printf("  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// 显示 IP 地址（RTM_NEWADDR）
void handle_addr(struct nlmsghdr *nlh) {
    struct ifaddrmsg *ifa = NLMSG_DATA(nlh);
    struct rtattr *attr = IFA_RTA(ifa);
    int len = IFA_PAYLOAD(nlh);

    char ifname[IF_NAMESIZE] = "";

    for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
        if (attr->rta_type == IFA_LOCAL) {
            char ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, RTA_DATA(attr), ip, sizeof(ip));
            if_indextoname(ifa->ifa_index, ifname);
            printf("  IP: %s (%s)\n", ip, ifname);
        }
    }
}

void send_request(int sock, int type) {
    char buf[BUFSIZE];
    struct sockaddr_nl sa = {0};
    sa.nl_family = AF_NETLINK;

    struct {
        struct nlmsghdr hdr;
        struct rtgenmsg gen;
    } req;

    memset(&req, 0, sizeof(req));
    req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    req.hdr.nlmsg_type = type;
    req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.hdr.nlmsg_seq = 1;
    req.gen.rtgen_family = AF_PACKET;

    struct iovec iov = {
        .iov_base = &req,
        .iov_len = req.hdr.nlmsg_len
    };

    struct msghdr msg = {
        .msg_name = &sa,
        .msg_namelen = sizeof(sa),
        .msg_iov = &iov,
        .msg_iovlen = 1
    };

    if (sendmsg(sock, &msg, 0) < 0) {
        perror("sendmsg");
        exit(EXIT_FAILURE);
    }
}

void receive_responses(int sock, int type) {
    char buf[BUFSIZE];

    while (1) {
        struct iovec iov = {
            .iov_base = buf,
            .iov_len = sizeof(buf)
        };
        struct sockaddr_nl sa;
        struct msghdr msg = {
            .msg_name = &sa,
            .msg_namelen = sizeof(sa),
            .msg_iov = &iov,
            .msg_iovlen = 1
        };

        ssize_t len = recvmsg(sock, &msg, 0);
        if (len < 0) {
            perror("recvmsg");
            exit(EXIT_FAILURE);
        }

        struct nlmsghdr *nlh;
        for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
            if (nlh->nlmsg_type == NLMSG_DONE)
                return;
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                fprintf(stderr, "Netlink error\n");
                exit(EXIT_FAILURE);
            }
            if (type == RTM_GETLINK && nlh->nlmsg_type == RTM_NEWLINK)
                handle_link(nlh);
            else if (type == RTM_GETADDR && nlh->nlmsg_type == RTM_NEWADDR)
                handle_addr(nlh);
        }
    }
}

int main() {
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_nl local = {0};
    local.nl_family = AF_NETLINK;
    local.nl_pid = getpid();

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        return 1;
    }

    // 获取接口名 + MAC
    send_request(sock, RTM_GETLINK);
    receive_responses(sock, RTM_GETLINK);

    // 获取 IP 地址
    send_request(sock, RTM_GETADDR);
    receive_responses(sock, RTM_GETADDR);

    close(sock);
    return 0;
}
