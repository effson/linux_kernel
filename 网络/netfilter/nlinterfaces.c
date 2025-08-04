#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define BUFSIZE 1024

struct nl_req_s {
  struct nlmsghdr hdr;
  struct rtgenmsg gen;
};
/* 
+----------------+
| nlmsghdr       |  (通用 Netlink 消息头部)
+----------------+
| Message Body   |  (特定类型的消息体，例如 rtgenmsg, ifinfomsg, rtmsg 等)
+----------------+
| Netlink Attributes | (可选的 TLV 格式属性，提供额外详细信息)
+----------------+
*/
void rtnetfilter_print_link(struct nlmsghdr *h) 
{
/* struct ifinfomsg 是 Linux 内核和用户空间之间
  通过 Netlink 协议交换网络接口信息时使用的数据结构，
  主要用于描述 网络设备（network interface）状态*/
  struct ifinfomsg *iface;
  /*Netlink Route（rtnetlink）消息中一个“Type-Length-Value (TLV)” 
  格式的单个属性头，用于封装附加字段，比如接口名、MAC 地址、MTU 等*/
  struct rtattr *attr; 
  int len = 0;
  iface = NLMSG_DATA(h);
  len = RTM_PAYLOAD(h);

  /* [ Netlink header ]
     [ struct ifinfomsg ]  ← 固定字段
     [ struct rtattr ]     ← attribute 1
     [ struct rtattr ]     ← attribute 2 
   */
  for (attr = IFLA_RTA(iface); RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) 
  {
    switch(attr->rta_type) {
        case IFLA_IFNAME: // 是否表示一个网络接口名称
          printf("Interface name: %s\n", (char *)RTA_DATA(attr));
          break;
        case IFLA_ADDRESS: // 网络接口的硬件地址 MAC
          const unsigned char *mac_addr = (unsigned char *)RTA_DATA(attr);
          if (RTA_PAYLOAD(attr) >= 6) { // 检查属性的有效载荷长度是否至少为6字节
            printf("  MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
          } else {
            printf("  MAC Address (invalid length): ");
            // 可以选择打印原始字节或进行其他错误处理
            for (int i = 0; i < RTA_PAYLOAD(attr); i++) {
              printf("%02x%s", mac_addr[i], (i == RTA_PAYLOAD(attr) - 1) ? "" : ":");
            }
            printf("\n");
          }
          break;
        default:
          break;
    }
  }
}

int main (int argc, char *argv[]) {
  struct sockaddr_nl nlsock;
  struct msghdr msg;
  struct iovec io;

  struct nl_req_s req;
  int s = 0, end = 0, len = 0;
  char buf[BUFSIZE];

  memset(&nlsock, 0, sizeof(nlsock));
  nlsock.nl_family = AF_NETLINK;
  nlsock.nl_groups = 0;
  s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (s < 0) {
    perror("netlink socket fail.\n");
    exit(1);
  }

  memset(&req, 0, sizeof(req));
  req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
  req.hdr.nlmsg_type = RTM_GETLINK; // 请求内核返回当前系统上所有网络接口（或指定接口）的详细信息
  req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP; //指示内核返回指定类型的所有现有对象
  req.hdr.nlmsg_seq = 1; // 序列号，用于排列消息处理
  req.hdr.nlmsg_pid = getpid();

  req.gen.rtgen_family = AF_INET; // ipv4

  memset(&io, 0, sizeof(io));
  io.iov_base = &req;
  io.iov_len = req.hdr.nlmsg_len;

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &io;
  msg.msg_iovlen = 1;
  msg.msg_name = &nlsock;
  msg.msg_namelen = sizeof(nlsock);

  s = sendmsg(s, &msg, 0);
  if (s < 0) {
    perror("sendmsg\n");
    exit(1);
  }

  while (!end) 
  {
    memset(buf, 0, BUFSIZE);
    msg.msg_iov->iov_base = buf;
    msg.msg_iov->iov_len = BUFSIZE;

    len = recvmsg(s, &msg, 0);
    if (len < 0) 
    {
      perror("recvmsg");
    }
    for (struct nlmsghdr*msg_ptr = (struct nlmsghdr*)buf; NLMSG_OK(msg_ptr, len); msg_ptr = NLMSG_NEXT(msg_ptr, len)) 
    {
      switch (msg_ptr->nlmsg_type)
      {
        case NLMSG_DONE:
          end ++;
          break;
        case RTM_NEWLINK:
          rtnetfilter_print_link(msg_ptr);
          break;
        default:
          printf("ignore msg: [type] %d, len = %d\n", msg_ptr->nlmsg_type, msg_ptr->nlmsg_len);
          break;
      }
    }
  }

  return 0;
}
