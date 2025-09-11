# 1.四次挥手

<img width="366" height="346" alt="image" src="https://github.com/user-attachments/assets/9bf85772-0020-4cfc-b577-85001080a0dd" />

### 2MSL(Maximum Segment Lifetime)
<mark>**MSL (Maximum Segment Lifetime)：一个报文在网络中能存活的最长时间**</mark>
- 确保最后一个 ACK 能够被对方收到
- 防止旧连接的延迟报文对新连接造成干扰
- 假如五元组相同，旧连接的报文还在网络里乱跑，新连接一建立，可能被当成新连接的数据，导致错误。所以必须等待 2MSL，确保所有残留报文都自然消亡

## 1.1 第一次挥手
### 客户端用户空间
- 应用程序调用 close(fd) 或 shutdown(fd, SHUT_WR)
- 系统调用进入内核后，会触发 TCP 协议栈的关闭流程

### 客户端内核空间
- ESTABLISHED → FIN_WAIT_1
- FIN 消耗一个序列号，TCP 把 FIN 当成占用 1 字节的数据
- TCP 会先确保发送缓冲区的数据已发出，然后才在最后一个包里带上 FIN
- 发送缓冲区里还有数据没发完，FIN 会延迟，直到数据发完再发


### 服务端内核
- 收到 FIN 包，TCP 栈会立即回一个 ACK（确认号=客户端 FIN 的序号+1）。
- 服务端状态机：ESTABLISHED → CLOSE_WAIT
- 内核记录 EOF：内核把这个“对端不再发送数据”的事件记录下来。
-之后如果应用层调用 read()，当读到缓冲区数据耗尽时，会返回 0，表示对端已经关闭写端

### 服务端用户空间
- 服务端进程不会立刻感知到 FIN（因为 read 可能还有缓冲区的数据）
- 当 recv()/read() 返回 0 时，应用层才知道对端关闭了发送方向
- 应用程序通常会：处理完剩余数据；调用 close() 或 shutdown() 关闭自己这边的连接，从而进入后续的挥手流程

## 1.2 第二次挥手

## 1.3 第三次挥手

## 1.4 第四次挥手
