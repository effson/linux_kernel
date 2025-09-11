# 1.四次挥手

<img width="366" height="346" alt="image" src="https://github.com/user-attachments/assets/9bf85772-0020-4cfc-b577-85001080a0dd" />

## 1.1 第一次挥手
### 客户端用户空间
- 应用程序调用 close(fd) 或 shutdown(fd, SHUT_WR)
- 系统调用进入内核后，会触发 TCP 协议栈的关闭流程

### 客户端内核空间
- ESTABLISHED → FIN_WAIT_1
- FIN 消耗一个序列号，TCP 把 FIN 当成占用 1 字节的数据
- TCP 会先确保发送缓冲区的数据已发出，然后才在最后一个包里带上 FIN
- 发送缓冲区里还有数据没发完，FIN 会延迟，直到数据发完再发
