# 1.背景
## 1.1 三次握手


## 1.2TCP特点


## 1.3 socket五元组
socket 的 五元组 (5-tuple) 是用来<mark>**唯一标识一条连接的五个元素，同一个五元组只允许建立一条连接**</mark>通常包括以下内容：
### 源 IP 地址 (Source IP Address)
发起连接的一方的 IP 地址。
### 目标 IP 地址 (Destination IP Address)
接收连接的一方的 IP 地址。
### 源端口号 (Source Port)
客户端操作系统分配的临时端口（ephemeral port）
### 目标端口号 (Destination Port)
服务端监听的端口，例如 80（HTTP）、3306（MySQL）
### 传输层协议 (Transport Protocol)
TCP， UDP


## 1.4数据包序列号
