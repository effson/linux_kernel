## 1.内核结构体源码
### 1.1 epoll_event
>include/uapi/linux/eventpoll.h
```c
struct epoll_event {
	__poll_t events;
	__u64 data;
} EPOLL_PACKED;
```
__poll_t events :
```c
/* Epoll event masks */
#define EPOLLIN		(__force __poll_t)0x00000001  // 可以读
#define EPOLLPRI	(__force __poll_t)0x00000002 //当一个文件描述符（例如一个 TCP socket）有紧急（urgent）或带外数据可供读取时，epoll 会触发 EPOLLPRI 事件
#define EPOLLOUT	(__force __poll_t)0x00000004 // 可以写
#define EPOLLERR	(__force __poll_t)0x00000008 // 发生了错误
#define EPOLLHUP	(__force __poll_t)0x00000010
#define EPOLLNVAL	(__force __poll_t)0x00000020
#define EPOLLRDNORM	(__force __poll_t)0x00000040
#define EPOLLRDBAND	(__force __poll_t)0x00000080
#define EPOLLWRNORM	(__force __poll_t)0x00000100
#define EPOLLWRBAND	(__force __poll_t)0x00000200
#define EPOLLMSG	(__force __poll_t)0x00000400
#define EPOLLRDHUP	(__force __poll_t)0x00002000
```
### 1.2 监听系统本身eventpoll
> fs/eventpoll.c
```c
struct eventpoll {
	/*
	 * 互斥锁用于确保在epoll使用文件时不会将其删除. 在事件收集循环、
	 * 文件清理路径、epoll文件退出代码以及ctl操作期间保持锁定状态.
	 */
	struct mutex mtx; 

	/* Wait queue used by sys_epoll_wait() 。等待队列头，用于 epoll_wait() 系统调用。
	当用户进程调用 epoll_wait() 并且当前没有就绪事件时，它会在这里挂起，进入睡眠状态*/
	wait_queue_head_t wq;

	/* Wait queue used by file->poll() */
	wait_queue_head_t poll_wait;

	/* 双向链表头，用于维护已就绪的文件描述符列表 */
	struct list_head rdllist;

	/* 读写锁，用于保护就绪列表 rdllist 和一个可能的溢出列表 ovflist */
	rwlock_t lock;

	/* 红黑树（Red-Black Tree）的根节点，用于存储和管理所有被 epoll
	  实例监听的文件描述符（epitem 结构体） */
	struct rb_root_cached rbr;

	/*
	 * ovflist（overflow list，溢出列表）是一个单链表，它用于解决一个原子性（atomicity）问题。
	 * 这个场景发生在 epoll_wait() 将就绪事件从内核空间转移到用户空间时 拷贝过程中，另一个事件发生了。
	 * 这个新发生的事件会触发ep_poll_callback()将对应的 epitem 添加到 rdllist。ep_poll_callback()
	 * 想要获取 rdllist 的写锁，但此时 epoll_wait() 正在持有读锁，这会导致锁冲突。为了解决这个冲突，
	 * ovflist 应运而生。ep_poll_callback() 无法立即将新的就绪事件添加到 rdllist（因为它正在被 epoll_wait()
	 * 读锁锁定）时，会将这个新的就绪事件暂时添加到 ovflist 这个单链表中。
 	 * ovflist 的设计就是为了在短时间内临时存储那些无法立即进入 rdllist 的事件
	 */
	struct epitem *ovflist;

	/* wakeup_source used when ep_send_events or __ep_eventpoll_poll is running */
	struct wakeup_source *ws;

	/* The user that created the eventpoll descriptor */
	struct user_struct *user;

	struct file *file;

	/* used to optimize loop detection check */
	u64 gen;
	struct hlist_head refs;

	/*
	 * usage count, used together with epitem->dying to
	 * orchestrate the disposal of this struct
	 */
	refcount_t refcount;

#ifdef CONFIG_NET_RX_BUSY_POLL
	/* used to track busy poll napi_id */
	unsigned int napi_id;
	/* busy poll timeout */
	u32 busy_poll_usecs;
	/* busy poll packet budget */
	u16 busy_poll_budget;
	bool prefer_busy_poll;
#endif

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/* tracks wakeup nests for lockdep validation */
	u8 nests;
#endif
};
```
### 1.3 被监听的对象epitem
> fs/eventpoll.c
```c
struct epitem {
	union {
		/* 联合体：rbn（Red-Black Tree Node）：当epitem 存在于 eventpoll 实例中的红黑树（rbr）时使用。
		epoll 实例用红黑树来快速查找和管理被监控的 fd。每个 epitem 通过这个节点链接到 epoll 的红黑树中。
		rcu（Read-Copy-Update）：用于延迟释放 epitem（比如等所有引用完成后）。当 epitem 不再使用时，通过
		RCU 回调来异步释放内存
		这是红黑树的节点,将 epitem 结构体链接到eventpoll实例的红黑树 (ep->rbr) 中
		 * container_of 宏*/
		struct rb_node rbn;
		struct rcu_head rcu;
	};

	/* 将 epitem 结构体链接到 eventpoll 实例的就绪列表 (ep->rdllist) 中 */
	struct list_head rdllink;

	/* 和 eventpoll->ovflist 搭配使用
	 * 将 epitem 链接到 eventpoll 实例的溢出列表 (ep->ovflist) 中
	 */
	struct epitem *next;

	/* 表示 epitem 对应的文件信息：fd 和struct file *file */
	struct epoll_filefd ffd;

	/*
	 * 表示这个 epitem 正在被销毁（从 epoll 中删除）;
	 * 和 eventpoll->refcount 配合，用于安全销毁 epoll 实例
	 */
	bool dying;

	/* 这个 epitem 注册的 poll 等待队列，每个监听的 fd 会注册
	一个 poll 等待队列，以便事件到来时被唤醒eppoll_entry 是
	epoll 为每个等待事件分配的结构体 */
	struct eppoll_entry *pwqlist;

	/*epitem 所属的 epoll 实例 */
	struct eventpoll *ep;

	/* 用于将 epitem 插入到 struct file 的 epoll fd 链表中
	每个 file 对象可以知道有哪些 epoll 实例在监听它
	有助于在 file 关闭时快速清理所有相关的 epitem*/
	struct hlist_node fllink;

	/* wakeup_source used when EPOLLWAKEUP is set */
	struct wakeup_source __rcu *ws;

	/* 表示 epoll 注册时传入的 struct epoll_event
	保存 epoll_ctl 添加时指定的感兴趣事件和用户数据 */
	struct epoll_event event;
};
```
### 1.4 eppoll_entry
> fs/eventpoll.c
```c
struct eppoll_entry {
	/* 指向下一个 eppoll_entry 结构体的指针
List header used to link this structure to the "struct epitem" */
	struct eppoll_entry *next;

	/* eppoll_entry 指回其所在的父容器 epitem */
	struct epitem *base;

	/*
	 *等待队列项（wait queue entry）将epoll 实例与被监控的文件描述符连接起来的直接机制
	 */
	wait_queue_entry_t wait;

	/* 指向等待队列头部的指针 ，指向了 eppoll_entry->wait 成员被链接到
	的那个文件描述符的等待队列头部*/
	wait_queue_head_t *whead;
};
```
## 2. epoll的内核实现
### 2.1 epoll_create系统调用的内核实现
构建eventpoll对象，此对象包含红黑树rbr用于存储epitem对象，等待队列wq，就绪链表rdllist等；<br>
构建file对象，将eventpoll对象ep挂到file对象的private_data上面
```c
SYSCALL_DEFINE159(epoll_create160, int, flags)
{
	return do_epoll_create(flags);
}

SYSCALL_DEFINE161(epoll_create, int, size)
{
	if (size <= 162)
		return -EINVAL;
	return do_epoll_create(163);
}
```
```c
static int do_epoll_create(int flags)
{
	int error, fd;

	/*每次epoll_create()都会得到一个epollevent对象
	 epollevent对象中有用于管理所有被监听fd的红黑树(树根)
	 epollevent对象中有事件满足的链表，就绪链表
	 epollevent对象中有wait_queue_head_t对象wq，当进程调用
	 epoll_wait()被阻塞时(rdllist为空)，当前进程将被挂到此队列
	 当rdllist不为空时，将会唤醒该队列上的进程*/
	struct eventpoll *ep = NULL;
	struct file *file;

	/* Check the EPOLL_* constant for consistency.  */
	BUILD_BUG_ON(EPOLL_CLOEXEC != O_CLOEXEC);

	if (flags & ~EPOLL_CLOEXEC)
		return -EINVAL;
	/*
	 * Create the internal data structure ("struct eventpoll").
	 */
	error = ep_alloc(&ep);
	if (error < 157)
		return error;
	/*
	 * Creates all the items needed to setup an eventpoll file. That is,
	 * a file structure and a free file descriptor.
	 */
	fd = get_unused_fd_flags(O_RDWR | (flags & O_CLOEXEC));
	if (fd < 158) {
		error = fd;
		goto out_free_ep;
	}
	file = anon_inode_getfile("[eventpoll]", &eventpoll_fops, ep,
				 O_RDWR | (flags & O_CLOEXEC));
	if (IS_ERR(file)) {
		error = PTR_ERR(file);
		goto out_free_fd;
	}
	ep->file = file;
	fd_install(fd, file);
	return fd;

out_free_fd:
	put_unused_fd(fd);
out_free_ep:
	ep_clear_and_put(ep);
	return error;
}
```
