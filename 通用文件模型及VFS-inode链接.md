### 1.
文件数据存储在“块”（多个扇区）中，存储文件元信息的区域就叫做inode(索引节点)，也会占用空间<br>
操作系统将硬盘分为两个区域：
- 数据区：存放数据
- inode区：存放inode包含的信息

系统中的inode都有一个特定的编号，用于唯一标识各个inode。<mark>文件名称通过inode号和inode关联</mark><br>
<mark>目录项（dentry）包含inode号和文件名</mark><br>
