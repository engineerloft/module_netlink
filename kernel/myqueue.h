#ifndef __MYQUEUE__
#define __MYQUEUE__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>

struct myts {
		u64 sec;
		u64 nsec;
		u64 seq;
		int valid;
		int tx_rx;
};

struct myqueue_element {
		struct list_head next;
		struct myts ts;
};

struct myqueue {
	struct list_head queue;
	spinlock_t lock;
};

struct myqueue_element * kmalloc_qe(struct timespec *ts, 
	int valid, int tx_rx, u64 seq);
void queue_init(struct myqueue *q);
int enqueue(struct myqueue *q, struct myqueue_element *qe);
struct myqueue_element *dequeue(struct myqueue *q);
int is_queue_empty(struct myqueue *q);
void kfree_queue(struct myqueue *q);
void sprintf_ts(char *buf, struct myqueue_element *qe);

#endif
