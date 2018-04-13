#include "nl_ts_queue.h"

struct nl_ts_queue_element * nl_ts_queue_kmalloc(struct nl_ts *ts)
{
	struct nl_ts_queue_element *qe = NULL;
	
	qe = kmalloc(sizeof(struct nl_ts_queue_element),
			GFP_ATOMIC);
	if(!qe)
		return NULL;
			
	(qe->ts).sec = ts->sec;
	(qe->ts).nsec = ts->nsec;
	(qe->ts).valid = ts->valid;
	(qe->ts).type = ts->type;
	(qe->ts).seq = ts->seq;
	(qe->ts).id = ts->id;
	(qe->ts).ahead = ts->ahead;
	
	return qe;
}

void nl_ts_queue_init(struct nl_ts_queue *q)
{
	INIT_LIST_HEAD(&q->queue);
	spin_lock_init(&q->lock);
}

int nl_ts_queue_enqueue(struct nl_ts_queue *q, struct nl_ts_queue_element *qe)
{	
	unsigned long flags;
	
	spin_lock_irqsave(&q->lock, flags);
	list_add_tail(&qe->next, &q->queue);
	spin_unlock_irqrestore(&q->lock, flags);
	
	return 0;
}

int nl_ts_queue_is_empty(struct nl_ts_queue *q)
{
	return list_empty(&q->queue);
}

struct nl_ts_queue_element *nl_ts_queue_dequeue(struct nl_ts_queue *q)
{
	struct nl_ts_queue_element *qe = NULL;
	unsigned long flags;
	
	spin_lock_irqsave(&q->lock, flags);
	if (!nl_ts_queue_is_empty(q)) {
		qe = list_first_entry(&q->queue, struct nl_ts_queue_element,
			next);
		list_del(&qe->next);
	}
	spin_unlock_irqrestore(&q->lock, flags);
	
	return qe;
}

void nl_ts_queue_kfree(struct nl_ts_queue *q)
{
	struct nl_ts_queue_element *qe = NULL;
	
	while (!nl_ts_queue_is_empty(q)) {
		qe = list_first_entry(&q->queue, struct nl_ts_queue_element,
			next);
		list_del(&qe->next);
		kfree(qe);
	}
}

static void nl_ts_queue_element_printk(struct nl_ts_queue_element *qe)
{
	if(!qe)
		return;
		
	printk("\n");
	printk("Queue element: \n");
	printk("sec: %llu \n", qe->ts.sec);
	printk("nsec: %llu \n", qe->ts.nsec);
	printk("seq: %llu \n", qe->ts.seq);
	printk("id: %u \n", qe->ts.id);
	printk("ahead: %d \n", qe->ts.ahead);
	printk("valid: %d \n", qe->ts.valid);
	printk("type: %d \n", qe->ts.type);
	printk("\n");
}

void nl_ts_queue_printk(struct nl_ts_queue *q)
{	
	struct list_head *i;
	
	list_for_each(i, &q->queue) {
		struct nl_ts_queue_element *qe = list_entry(i, 
			struct nl_ts_queue_element, next);
		nl_ts_queue_element_printk(qe);
	}
}
