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

inline struct myqueue_element * kmalloc_qe(struct timespec *ts, 
	int valid, int tx_rx, u64 seq)
{
	struct myqueue_element *qe = NULL;
	
	qe = kmalloc(sizeof(struct myqueue_element),
			GFP_ATOMIC);
	if(!qe)
		return NULL;
			
	(qe->ts).sec = ts->tv_sec;
	(qe->ts).nsec = ts->tv_nsec;
	(qe->ts).valid = valid;
	(qe->ts).tx_rx = tx_rx;
	(qe->ts).seq = seq;
	
	return qe;
}

inline void queue_init(struct myqueue *q)
{
	INIT_LIST_HEAD(&q->queue);
	spin_lock_init(&q->lock);
}

inline int enqueue(struct myqueue *q, struct myqueue_element *qe)
{	
	unsigned long flags;
	
	spin_lock_irqsave(&q->lock, flags);
	list_add_tail(&qe->next, &q->queue);
	spin_unlock_irqrestore(&q->lock, flags);
	
	return 0;
}

inline struct myqueue_element *dequeue(struct myqueue *q)
{
	struct myqueue_element *qe = NULL;
	unsigned long flags;
	
	spin_lock_irqsave(&q->lock, flags);
	qe = list_first_entry(&q->queue, struct myqueue_element,
		next);
	list_del(&qe->next);
	spin_unlock_irqrestore(&q->lock, flags);
	
	return qe;
}

inline int is_queue_empty(struct myqueue *q)
{
	return list_empty(&q->queue);
}

inline void kfree_queue(struct myqueue *q)
{
	struct myqueue_element *qe = NULL;
	
	while (!is_queue_empty(q)) {
		qe = list_first_entry(&q->queue, struct myqueue_element,
			next);
		list_del(&qe->next);
		kfree(qe);
	}
}

inline void sprintf_ts(char *buf, struct myqueue_element *qe)
{
	sprintf(buf,"%.2llu:%.2llu:%.2llu:%llu:v%d:t%d:s%llu",
				(qe->ts.sec / 3600) % (24),
				(qe->ts.sec / 60) % (60),
				qe->ts.sec % 60,
				qe->ts.nsec / 1000,
				qe->ts.valid,
				qe->ts.tx_rx,
				qe->ts.seq);
}
#endif
