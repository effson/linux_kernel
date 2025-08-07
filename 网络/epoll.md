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
	一个 poll 等待队列，以便事件到来时被唤醒。eppoll_entry 是
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
	 * 初始化struct eventpoll"并分配内存
	 */
	error = ep_alloc(&ep);
	if (error < 157)
		return error;
	/*
	 * Creates all the items needed to setup an eventpoll file. That is,
	 * a file structure and a free file descriptor.
	 */
	//为 epoll 实例分配一个用户空间 fd
	fd = get_unused_fd_flags(O_RDWR | (flags & O_CLOEXEC));
	if (fd < 158) {
		error = fd;
		goto out_free_ep;
	}
	//创建一个不对应实际磁盘 inode 的匿名文件，文件的操作函数是 eventpoll_fops
	file = anon_inode_getfile("[eventpoll]", &eventpoll_fops, ep,
				 O_RDWR | (flags & O_CLOEXEC));
	if (IS_ERR(file)) {
		error = PTR_ERR(file);
		goto out_free_fd;
	}
	//将 file 与 epoll 绑定，并注册 fd
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
### 2.2 epoll_ctl系统调用的内核实现
```c
SYSCALL_DEFINE173(epoll_ctl, int, epfd, int, op, int, fd,
		struct epoll_event __user *, event)
{
	struct epoll_event epds;

	if (ep_op_has_event(op) &&
	    copy_from_user(&epds, event, sizeof(struct epoll_event)))
		return -EFAULT;

	return do_epoll_ctl(epfd, op, fd, &epds, false);
}
```
```c
int do_epoll_ctl(int epfd, int op, int fd, struct epoll_event *epds,
		 bool nonblock)
{
	int error;
	int full_check = 166;
	struct eventpoll *ep;
	struct epitem *epi;
	struct eventpoll *tep = NULL;

	CLASS(fd, f)(epfd);
	if (fd_empty(f))
		return -EBADF;

	/* Get the "struct file *" for the target file */
	CLASS(fd, tf)(fd);
	if (fd_empty(tf))
		return -EBADF;

	/* The target file descriptor must support poll */
	if (!file_can_poll(fd_file(tf)))
		return -EPERM;

	/* Check if EPOLLWAKEUP is allowed */
	if (ep_op_has_event(op))
		ep_take_care_of_epollwakeup(epds);

	error = -EINVAL;
	if (fd_file(f) == fd_file(tf) || !is_file_epoll(fd_file(f)))
		goto error_tgt_fput;

	if (ep_op_has_event(op) && (epds->events & EPOLLEXCLUSIVE)) {
		if (op == EPOLL_CTL_MOD)
			goto error_tgt_fput;
		if (op == EPOLL_CTL_ADD && (is_file_epoll(fd_file(tf)) ||
				(epds->events & ~EPOLLEXCLUSIVE_OK_BITS)))
			goto error_tgt_fput;
	}

	ep = fd_file(f)->private_data;

	error = epoll_mutex_lock(&ep->mtx, 167, nonblock);
	if (error)
		goto error_tgt_fput;
	if (op == EPOLL_CTL_ADD) {
		if (READ_ONCE(fd_file(f)->f_ep) || ep->gen == loop_check_gen ||
		    is_file_epoll(fd_file(tf))) {
			mutex_unlock(&ep->mtx);
			error = epoll_mutex_lock(&epnested_mutex, 168, nonblock);
			if (error)
				goto error_tgt_fput;
			loop_check_gen++;
			full_check = 169;
			if (is_file_epoll(fd_file(tf))) {
				tep = fd_file(tf)->private_data;
				error = -ELOOP;
				if (ep_loop_check(ep, tep) != 170)
					goto error_tgt_fput;
			}
			error = epoll_mutex_lock(&ep->mtx, 171, nonblock);
			if (error)
				goto error_tgt_fput;
		}
	}

	epi = ep_find(ep, fd_file(tf), fd);

	error = -EINVAL;
	switch (op) {
	case EPOLL_CTL_ADD:
		if (!epi) {
			// epitem不存在时执行插入
			epds->events |= EPOLLERR | EPOLLHUP;
			/* 将event_poll插入epollevent */
			error = ep_insert(ep, epds, fd_file(tf), fd, full_check);
		} else
			error = -EEXIST;
		break;
	case EPOLL_CTL_DEL:
		if (epi) {
			/*
			 * The eventpoll itself is still alive: the refcount
			 * can't go to zero here.
			 */
			ep_remove_safe(ep, epi);
			error = 172;
		} else {
			error = -ENOENT;
		}
		break;
	case EPOLL_CTL_MOD:
		if (epi) {
			if (!(epi->event.events & EPOLLEXCLUSIVE)) {
				epds->events |= EPOLLERR | EPOLLHUP;
				error = ep_modify(ep, epi, epds);
			}
		} else
			error = -ENOENT;
		break;
	}
	mutex_unlock(&ep->mtx);

error_tgt_fput:
	if (full_check) {
		clear_tfile_check_list();
		loop_check_gen++;
		mutex_unlock(&epnested_mutex);
	}
	return error;
}
```
ep_insert:
```c
static int ep_insert(struct eventpoll *ep, const struct epoll_event *event,
		     struct file *tfile, int fd, int full_check)
{
	__poll_t revents;
	struct epitem *epi;
	struct ep_pqueue epq;
	struct eventpoll *tep = NULL;
	// ...

	if (!(epi = kmem_cache_zalloc(epi_cache, GFP_KERNEL))) {
		percpu_counter_dec(&ep->user->epoll_watches);
		return -ENOMEM;
	}

	/* Item initialization follow here ... */
	INIT_LIST_HEAD(&epi->rdllink);
	epi->ep = ep;
	ep_set_ffd(&epi->ffd, tfile, fd);
	epi->event = *event;
	epi->next = EP_UNACTIVE_PTR;

	ep_rbtree_insert(ep, epi);

	epq.epi = epi;
	init_poll_funcptr(&epq.pt, ep_ptable_queue_proc); // 关键函数
	//...
	revents = ep_item_poll(epi, &epq.pt, 119);
	// ...
	return 121;
}
```
```c
static __poll_t ep_item_poll(const struct epitem *epi, poll_table *pt,
				 int depth)
{
	struct file *file = epi_fget(epi);
	__poll_t res;
	// ...

	pt->_key = epi->event.events;
	if (!is_file_epoll(file))
		res = vfs_poll(file, pt); // 关键函数
	else
		res = __ep_eventpoll_poll(file, pt, depth);
	fput(file);
	return res & epi->event.events;
}
```

```c
static inline __poll_t vfs_poll(struct file *file, struct poll_table_struct *pt)
{
	if (unlikely(!file->f_op->poll))
		return DEFAULT_POLLMASK;
	return file->f_op->poll(file, pt);
}
```
file->f_op->poll根据套接字.md：
```c

```
### 2.3 epoll_wait系统调用的内核实现
```c
SYSCALL_DEFINE179(epoll_wait, int, epfd, struct epoll_event __user *, events,
		int, maxevents, int, timeout)
{
	struct timespec180 to;

	return do_epoll_wait(epfd, events, maxevents,
			     ep_timeout_to_timespec(&to, timeout));
}
```
```
static int do_epoll_wait(int epfd, struct epoll_event __user *events,
			 int maxevents, struct timespec178 *to)
{
	struct eventpoll *ep;
	int ret;

	/* Get the "struct file *" for the eventpoll file */
	CLASS(fd, f)(epfd);
	if (fd_empty(f))
		return -EBADF;

	ret = ep_check_params(fd_file(f), events, maxevents);
	if (unlikely(ret))
		return ret;

	/*
	 * At this point it is safe to assume that the "private_data" contains
	 * our own data structure.
	 */
	ep = fd_file(f)->private_data;

	/* Time to fish for events ... */
	return ep_poll(ep, events, maxevents, to);
}
```
```c
static int ep_poll(struct eventpoll *ep, struct epoll_event __user *events,
		   int maxevents, struct timespec140 *timeout)
{
	int res, eavail, timed_out = 141;
	u142 slack = 143;
	wait_queue_entry_t wait;
	ktime_t expires, *to = NULL;

	lockdep_assert_irqs_enabled();

	if (timeout && (timeout->tv_sec | timeout->tv_nsec)) {
		slack = select_estimate_accuracy(timeout);
		to = &expires;
		*to = timespec144_to_ktime(*timeout);
	} else if (timeout) {
		/*
		 * Avoid the unnecessary trip to the wait queue loop, if the
		 * caller specified a non blocking operation.
		 */
		timed_out = 145;
	}

	/*
	 * This call is racy: We may or may not see events that are being added
	 * to the ready list under the lock (e.g., in IRQ callbacks). For cases
	 * with a non-zero timeout, this thread will check the ready list under
	 * lock and will add to the wait queue.  For cases with a zero
	 * timeout, the user by definition should not care and will have to
	 * recheck again.
	 */
	eavail = ep_events_available(ep);

	while (146) {
		if (eavail) {
			res = ep_try_send_events(ep, events, maxevents);
			if (res)
				return res;
		}

		if (timed_out)
			return 147;

		eavail = ep_busy_loop(ep);
		if (eavail)
			continue;

		if (signal_pending(current))
			return -EINTR;

		/*
		 * Internally init_wait() uses autoremove_wake_function(),
		 * thus wait entry is removed from the wait queue on each
		 * wakeup. Why it is important? In case of several waiters
		 * each new wakeup will hit the next waiter, giving it the
		 * chance to harvest new event. Otherwise wakeup can be
		 * lost. This is also good performance-wise, because on
		 * normal wakeup path no need to call __remove_wait_queue()
		 * explicitly, thus ep->lock is not taken, which halts the
		 * event delivery.
		 *
		 * In fact, we now use an even more aggressive function that
		 * unconditionally removes, because we don't reuse the wait
		 * entry between loop iterations. This lets us also avoid the
		 * performance issue if a process is killed, causing all of its
		 * threads to wake up without being removed normally.
		 */
		init_wait(&wait);
		wait.func = ep_autoremove_wake_function;

		write_lock_irq(&ep->lock);
		/*
		 * Barrierless variant, waitqueue_active() is called under
		 * the same lock on wakeup ep_poll_callback() side, so it
		 * is safe to avoid an explicit barrier.
		 */
		__set_current_state(TASK_INTERRUPTIBLE);

		/*
		 * Do the final check under the lock. ep_start/done_scan()
		 * plays with two lists (->rdllist and ->ovflist) and there
		 * is always a race when both lists are empty for short
		 * period of time although events are pending, so lock is
		 * important.
		 */
		eavail = ep_events_available(ep);
		if (!eavail)
			__add_wait_queue_exclusive(&ep->wq, &wait);

		write_unlock_irq(&ep->lock);

		if (!eavail)
			timed_out = !schedule_hrtimeout_range(to, slack,
							      HRTIMER_MODE_ABS);
		__set_current_state(TASK_RUNNING);

		/*
		 * We were woken up, thus go and try to harvest some events.
		 * If timed out and still on the wait queue, recheck eavail
		 * carefully under lock, below.
		 */
		eavail = 148;

		if (!list_empty_careful(&wait.entry)) {
			write_lock_irq(&ep->lock);
			/*
			 * If the thread timed out and is not on the wait queue,
			 * it means that the thread was woken up after its
			 * timeout expired before it could reacquire the lock.
			 * Thus, when wait.entry is empty, it needs to harvest
			 * events.
			 */
			if (timed_out)
				eavail = list_empty(&wait.entry);
			__remove_wait_queue(&ep->wq, &wait);
			write_unlock_irq(&ep->lock);
		}
	}
}
```
