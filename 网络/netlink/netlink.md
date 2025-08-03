```bash
root@worker02:/home/jeff/network/netlink# make
make -C /lib/modules/6.8.0-64-generic/build M=/home/jeff/network/netlink modules
make[1]: Entering directory '/usr/src/linux-headers-6.8.0-64-generic'
warning: the compiler differs from the one used to build the kernel
  The kernel was built by: x86_64-linux-gnu-gcc-13 (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
  You are using:           gcc-13 (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
  CC [M]  /home/jeff/network/netlink/netlink.o
  MODPOST /home/jeff/network/netlink/Module.symvers
  CC [M]  /home/jeff/network/netlink/netlink.mod.o
  LD [M]  /home/jeff/network/netlink/netlink.ko
  BTF [M] /home/jeff/network/netlink/netlink.ko
Skipping BTF generation for /home/jeff/network/netlink/netlink.ko due to unavailability of vmlinux
make[1]: Leaving directory '/usr/src/linux-headers-6.8.0-64-generic'
```

```bash
root@worker02:/home/jeff/network/netlink# insmod netlink.ko
root@worker02:/home/jeff/network/netlink# dmesg
[76728.630888] netlink: loading out-of-tree module taints kernel.
[76728.631305] netlink: module verification failed: signature and/or required key missing - tainting kernel
[76728.651297] Netlink socket created successfully
```
