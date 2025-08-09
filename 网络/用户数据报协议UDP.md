### 1.UDP报头
<img width="869" height="333" alt="image" src="https://github.com/user-attachments/assets/321b49e1-2de9-425a-8e3a-807b34d06fc0" />

### 2.内核源码
> include/uapi/linux/udp.h
```c
struct udphdr {
	__be16	source;
	__be16	dest;
	__be16	len;
	__sum16	check;
};
```
