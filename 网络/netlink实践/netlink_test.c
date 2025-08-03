#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/netlink.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#define NETLINK_TEST 30
#define MSG_LEN 125
#define MAX_PLOAD 125

typedef struct _user_msg_info {
    struct nlmsghdr nlhdr;
    char msg[MSG_LEN]; 
}user_msg_info;

int main(int argc, char **argv) {
    int skfd;
    int ret;
    user_msg_info u_info;
    socklen_t len;
    struct nlmsghdr *nlh = NULL;
    struct sockaddr_nl saddr, daddr;
    char *umsg = "hello from jeff to netlink module";

    skfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_TEST);
    if (skfd < 0) {
        perror("\n Error:create socket\n");
        return 0;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.nl_family = AF_NETLINK;
    saddr.nl_pid = getpid();
    saddr.nl_groups = 0;

    if (bind(skfd, (struct sockaddr*)&saddr, sizeof(saddr)) != 0) {
        perror("\n Error:blind\n");
        close(skfd);
        return -ENOMEM;
    }

    memset(&daddr, 0, sizeof(daddr));
    daddr.nl_family = AF_NETLINK;
    daddr.nl_pid = 0;
    daddr.nl_groups = 0;

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PLOAD));
    memset(nlh, 0, sizeof(struct nlmsghdr));

    nlh->nlmsg_len = NLMSG_SPACE(MAX_PLOAD);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = saddr.nl_pid;

    memcpy(NLMSG_DATA(nlh), umsg, strlen(umsg));

    ret = sendto(skfd, nlh, nlh->nlmsg_len, 0, 
        (struct sockaddr*)&daddr, sizeof(struct sockaddr_nl));
    if (!ret) {
        perror("\n Error:sendto\n");
        close(skfd);
        exit(1);
    }
    printf("\n Application --> sendto kernel : %s\n", umsg);

    memset(&u_info, 0, sizeof(u_info));
    len = sizeof(struct sockaddr_nl);
    ret = recvfrom(skfd, &u_info, sizeof(user_msg_info), 
        0, (struct sockaddr*)&daddr, &len);
    if (!ret) {
        perror("\n Error:recvfrom\n");
        close(skfd);
        exit(1);
    }
    printf("\n Application <-- recvfrom kernel : %s\n", u_info.msg);

    return 0;
}
