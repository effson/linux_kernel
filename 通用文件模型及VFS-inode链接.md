### 1.
文件数据存储在“块”（多个扇区）中，存储文件元信息的区域就叫做inode(索引节点)，也会占用空间<br>
操作系统将硬盘分为两个区域：
- 数据区：存放数据
- inode区：存放inode包含的信息

系统中的inode都有一个特定的编号，用于唯一标识各个inode。<mark>文件名称通过inode号和inode关联</mark><br>
<mark>目录项（dentry）包含inode号和文件名</mark><br>
i_op指向inode_operation,操作目录和文件属性<br>
i_fop指向文件操作集合file_operations，用来访问文件的数据<br>

```c
struct inode_operations {
	// 在某个目录 inode 下，查找特定名字（由 dentry 表示）对应的子文件或子目录的 inode
	struct dentry * (*lookup) (struct inode *,struct dentry *, unsigned int);
	const char * (*get_link) (struct dentry *, struct inode *, struct delayed_call *);
	int (*permission) (struct mnt_idmap *, struct inode *, int);
	struct posix_acl * (*get_inode_acl)(struct inode *, int, bool);

	int (*readlink) (struct dentry *, char __user *,int);

	// 系统调用 open() 配合 O_CREAT 标志执行时所触发的函数，dir表示在哪个目录下创建文件（父目录）
	// dentry表示新建文件的名字（以及将绑定 inode），excl表示 O_EXCL 是否设置（用于确保独占创建）
	int (*create) (struct mnt_idmap *, struct inode *,struct dentry *,
		       umode_t, bool);
	int (*link) (struct dentry *,struct inode *,struct dentry *);
	int (*unlink) (struct inode *,struct dentry *);
	int (*symlink) (struct mnt_idmap *, struct inode *,struct dentry *,
			const char *);
	// mkdir() 系统调用，返回值是 struct dentry *，需要用 d_instantiate() 绑定新 inode
	struct dentry *(*mkdir) (struct mnt_idmap *, struct inode *,
				 struct dentry *, umode_t);
	int (*rmdir) (struct inode *,struct dentry *);
	int (*mknod) (struct mnt_idmap *, struct inode *,struct dentry *,
		      umode_t,dev_t);
	int (*rename) (struct mnt_idmap *, struct inode *, struct dentry *,
			struct inode *, struct dentry *, unsigned int);
	int (*setattr) (struct mnt_idmap *, struct dentry *, struct iattr *);
	int (*getattr) (struct mnt_idmap *, const struct path *,
			struct kstat *, u32, unsigned int);
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start,
		      u64 len);
	int (*update_time)(struct inode *, int);
	int (*atomic_open)(struct inode *, struct dentry *,
			   struct file *, unsigned open_flag,
			   umode_t create_mode);
	int (*tmpfile) (struct mnt_idmap *, struct inode *,
			struct file *, umode_t);
	struct posix_acl *(*get_acl)(struct mnt_idmap *, struct dentry *,
				     int);
	int (*set_acl)(struct mnt_idmap *, struct dentry *,
		       struct posix_acl *, int);
	int (*fileattr_set)(struct mnt_idmap *idmap,
			    struct dentry *dentry, struct fileattr *fa);
	int (*fileattr_get)(struct dentry *dentry, struct fileattr *fa);
	struct offset_ctx *(*get_offset_ctx)(struct inode *inode);
} ____cacheline_aligned;
```
