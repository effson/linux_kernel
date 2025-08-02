## 1.Netlink 的基本概念
### 1.1 介绍
Netlink 是一种基于 socket 的 IPC（进程间通信）机制，通过一种类似 BSD socket 的方式，让用户空间程序可以向内核发送请求或接收内核的通知。
### 1.2 源码 
在代码层面，它是通过 AF_NETLINK 协议族实现的，使用的 API 是 socket()、bind()、sendmsg()、recvmsg() 等<br>
在 net/netlink源码目录下
<img width="144" height="179" alt="image" src="https://github.com/user-attachments/assets/245aabc1-38e6-48c9-87fa-fb4621b713fb" />

## 2.
