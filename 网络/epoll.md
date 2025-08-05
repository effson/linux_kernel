## 内核源码
### epoll_event
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
### 监听系统本身eventpoll
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
### 被监听的对象epitem
> fs/eventpoll.c
```c
struct epitem {
	union {
		/* 这是红黑树的节点,将 epitem 结构体链接到eventpoll实例的红黑树 (ep->rbr) 中
		 * container_of 宏*/
		struct rb_node rbn;
		/* Used to free the struct epitem */
		struct rcu_head rcu;
	};

	/* 将 epitem 结构体链接到 eventpoll 实例的就绪列表 (ep->rdllist) 中 */
	struct list_head rdllink;

	/*
	 * 将 epitem 链接到 eventpoll 实例的溢出列表 (ep->ovflist) 中
	 */
	struct epitem *next;

	/* The file descriptor information this item refers to */
	struct epoll_filefd ffd;

	/*
	 * Protected by file->f_lock, true for to-be-released epitem already
	 * removed from the "struct file" items list; together with
	 * eventpoll->refcount orchestrates "struct eventpoll" disposal
	 */
	bool dying;

	/* List containing poll wait queues */
	struct eppoll_entry *pwqlist;

	/* The "container" of this item */
	struct eventpoll *ep;

	/* List header used to link this item to the "struct file" items list */
	struct hlist_node fllink;

	/* wakeup_source used when EPOLLWAKEUP is set */
	struct wakeup_source __rcu *ws;

	/* The structure that describe the interested events and the source fd */
	struct epoll_event event;
};
```
