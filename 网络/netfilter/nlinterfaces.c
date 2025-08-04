#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define BUFSIZE 1024

struct nl_req_s {
  struct nlmsghdr hdr;
  struct rtgenmsg gen;
};

void rtnetfilter_print_link(struct nlmsghdr *h) {
  
}
