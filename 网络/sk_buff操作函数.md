> include/linux/tcp.h<br>
> include/linux/udp.h<br>
获取tcp和udp头部：
```c
static inline struct tcphdr *tcp_hdr(const struct sk_buff *skb)
{
	return (struct tcphdr *)skb_transport_header(skb);
}
```
```c
static inline struct udphdr *udp_hdr(const struct sk_buff *skb)
{
	return (struct udphdr *)skb_transport_header(skb);
}
```
<br>
<br>
> include/linux/skbuff.h

```c
// 传输层首部指针计算方式
static inline unsigned char *skb_transport_header(const struct sk_buff *skb)
{
	DEBUG_NET_WARN_ON_ONCE(!skb_transport_header_was_set(skb));
	return skb->head + skb->transport_header;
}
/*当前 skb->data 指向的位置作为 TCP/UDP 层协议头（transport layer header）的位置，
  并记录在 skb->transport_header 中*/ 
static inline void skb_reset_transport_header(struct sk_buff *skb)
{
	long offset = skb->data - skb->head;

	DEBUG_NET_WARN_ON_ONCE(offset != (typeof(skb->transport_header))offset);
	skb->transport_header = offset;
}
```
```c
// 分配一个 sk_buff 结构和数据缓冲区
static inline struct sk_buff *alloc_skb(unsigned int size, gfp_t priority)
{
    return __alloc_skb(size, priority, 0, NUMA_NO_NODE);
}
```
```c
// 获取剩余空间大小（end - tail）
static inline int skb_tailroom(const struct sk_buff *skb)
{
    return skb_is_nonlinear(skb) ? 0 : skb->end - skb->tail;
}
```
```c
// 返回当前 skb 中 data 指针与 head 指针之间的字节数，即头部预留空间的大小（headroom）
static inline unsigned int skb_headroom(const struct sk_buff *skb)
{
    return skb->data - skb->head;
}
```
<br>
<br>
> net/core/stbuff.c
```c
// 浅拷贝（共享数据区，复制结构体）
struct sk_buff *skb_clone(struct sk_buff *skb, gfp_t gfp_mask)
{
	struct sk_buff_fclones *fclones = container_of(skb,
						       struct sk_buff_fclones,
						       skb1);
	struct sk_buff *n;

	if (skb_orphan_frags(skb, gfp_mask))
		return NULL;

	if (skb->fclone == SKB_FCLONE_ORIG &&
	    refcount_read(&fclones->fclone_ref) == 1) {
		n = &fclones->skb2;
		refcount_set(&fclones->fclone_ref, 2);
		n->fclone = SKB_FCLONE_CLONE;
	} else {
		if (skb_pfmemalloc(skb))
			gfp_mask |= __GFP_MEMALLOC;

		n = kmem_cache_alloc(net_hotdata.skbuff_cache, gfp_mask);
		if (!n)
			return NULL;

		n->fclone = SKB_FCLONE_UNAVAILABLE;
	}

	return __skb_clone(n, skb);
}
```
```c
/*将用户态传来的“用户页数据（user buffers）”复制成内核可控的 page frags
	（页片段）并附加到 skb 中，形成非线性 skb（即 data_len > 0）*/
int skb_copy_ubufs(struct sk_buff *skb, gfp_t gfp_mask)
{
	int num_frags = skb_shinfo(skb)->nr_frags;
	struct page *page, *head = NULL;
	int i, order, psize, new_frags;
	u32 d_off;

	if (skb_shared(skb) || skb_unclone(skb, gfp_mask))
		return -EINVAL;

	if (!skb_frags_readable(skb))
		return -EFAULT;

	if (!num_frags)
		goto release;

	/* We might have to allocate high order pages, so compute what minimum
	 * page order is needed.
	 */
	order = 0;
	while ((PAGE_SIZE << order) * MAX_SKB_FRAGS < __skb_pagelen(skb))
		order++;
	psize = (PAGE_SIZE << order);

	new_frags = (__skb_pagelen(skb) + psize - 1) >> (PAGE_SHIFT + order);
	for (i = 0; i < new_frags; i++) {
		page = alloc_pages(gfp_mask | __GFP_COMP, order);
		if (!page) {
			while (head) {
				struct page *next = (struct page *)page_private(head);
				put_page(head);
				head = next;
			}
			return -ENOMEM;
		}
		set_page_private(page, (unsigned long)head);
		head = page;
	}

	page = head;
	d_off = 0;
	for (i = 0; i < num_frags; i++) {
		skb_frag_t *f = &skb_shinfo(skb)->frags[i];
		u32 p_off, p_len, copied;
		struct page *p;
		u8 *vaddr;

		skb_frag_foreach_page(f, skb_frag_off(f), skb_frag_size(f),
				      p, p_off, p_len, copied) {
			u32 copy, done = 0;
			vaddr = kmap_atomic(p);

			while (done < p_len) {
				if (d_off == psize) {
					d_off = 0;
					page = (struct page *)page_private(page);
				}
				copy = min_t(u32, psize - d_off, p_len - done);
				memcpy(page_address(page) + d_off,
				       vaddr + p_off + done, copy);
				done += copy;
				d_off += copy;
			}
			kunmap_atomic(vaddr);
		}
	}

	/* skb frags release userspace buffers */
	for (i = 0; i < num_frags; i++)
		skb_frag_unref(skb, i);

	/* skb frags point to kernel buffers */
	for (i = 0; i < new_frags - 1; i++) {
		__skb_fill_netmem_desc(skb, i, page_to_netmem(head), 0, psize);
		head = (struct page *)page_private(head);
	}
	__skb_fill_netmem_desc(skb, new_frags - 1, page_to_netmem(head), 0,
			       d_off);
	skb_shinfo(skb)->nr_frags = new_frags;

release:
	skb_zcopy_clear(skb, false);
	return 0;
}
```
```c
// 重新分配一个新的 skb，并为其保留指定大小的 headroom（头部空间），同时复制原始数据和元信息
struct sk_buff *skb_realloc_headroom(struct sk_buff *skb, unsigned int headroom)
{
	struct sk_buff *skb2;
	int delta = headroom - skb_headroom(skb);

	if (delta <= 0)
		skb2 = pskb_copy(skb, GFP_ATOMIC);
	else {
		skb2 = skb_clone(skb, GFP_ATOMIC);
		if (skb2 && pskb_expand_head(skb2, SKB_DATA_ALIGN(delta), 0,
					     GFP_ATOMIC)) {
			kfree_skb(skb2);
			skb2 = NULL;
		}
	}
	return skb2;
}
```
