/* 
 * This file is part of the module_netlink project 
 * (https://github.com/engineerloft/module_netlink).
 * Copyright (c) 2018 Engineer Loft.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
  
#define NETLINK_CUSTOM_PROTO 31

#define TIMER_PERIOD 1*HZ
#define TX_TS_PERIOD 5
#define RX_TS_PERIOD 5
static int mytimer_events = 0;
static u64 tx_seq = 0;
static u64 rx_seq = 0;
static void mytimer_handler(unsigned long data);
DEFINE_TIMER(mytimer, mytimer_handler, 0, 0);

struct sock *nl_sk = NULL;
struct timespec ts_current;

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

static struct list_head rx_queue;
static struct list_head tx_queue;

static void myprintk_ts(struct timespec *ts)
{
	printk("%.2lu:%.2lu:%.2lu:%.6lu \n",
		(ts->tv_sec / 3600) % (24),
		(ts->tv_sec / 60) % (60),
		ts->tv_sec % 60,
		ts->tv_nsec / 1000);
}

static struct myqueue_element * kmalloc_qe(struct timespec *ts, 
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

static int enqueue(struct list_head *q, struct myqueue_element *qe)
{	
	list_add_tail(&qe->next, q);
	
	return 0;
}

static struct myqueue_element *dequeue(struct list_head *q)
{
	struct myqueue_element *qe = NULL;
	
	qe = list_first_entry(q, struct myqueue_element,
		next);
	list_del(&qe->next);
	
	return qe;
}

static void kfree_queue(struct list_head *q)
{
	struct myqueue_element *qe = NULL;
	
	while (!list_empty(q)) {
		qe = list_first_entry(q, struct myqueue_element,
			next);
		list_del(&qe->next);
		kfree(qe);
	}
}

static void sprintf_ts(char *buf, struct myqueue_element *qe)
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

static void mytimer_handler(unsigned long data)
{
		struct myqueue_element *qe = NULL;
		
		getnstimeofday(&ts_current);

		if (mytimer_events % TX_TS_PERIOD == 0) {
			qe = kmalloc_qe(&ts_current, 1, 0, tx_seq);
			enqueue(&tx_queue, qe);
			tx_seq++;
		}
			
		if (mytimer_events % RX_TS_PERIOD == 0) {
			qe = kmalloc_qe(&ts_current, 1, 1, rx_seq);
			enqueue(&rx_queue, qe);
			rx_seq++;
		}
		
		mytimer_events++;

		mod_timer(&mytimer, jiffies + TIMER_PERIOD);
}

#define MSG_SIZE_MAX 256
static void netlink_f_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int pid;
	struct sk_buff *skb_out;
	int msg_size;
	char msg[MSG_SIZE_MAX];
	int res;
	//struct timespec *ts = &(ts_current);
	char *recv_buf = NULL;
	char *error_string = "ERROR";
	char *queue_empty = "QUEUE EMPTY";
	int rx_queue_cmd = 0;
	int tx_queue_cmd = 0;
	int queue_cmd = 0;

	msg_size = strlen(msg);
	nlh = (struct nlmsghdr*)skb->data;
	pid = nlh->nlmsg_pid; /*pid of sending process */
	
	recv_buf = (char *) nlmsg_data(nlh);
	
	rx_queue_cmd = !strcmp("GETTS_RX",recv_buf);
	tx_queue_cmd = !strcmp("GETTS_TX",recv_buf);
	queue_cmd = rx_queue_cmd || tx_queue_cmd;
	
	if (queue_cmd) {
			struct myqueue_element *qe = NULL;
			
			if(rx_queue_cmd) {
				if (!list_empty(&rx_queue)) {
					qe = dequeue(&rx_queue);
					sprintf_ts(msg,qe);
				} else {
					strcpy(msg,queue_empty);
				}
			}
			else {
				if (!list_empty(&tx_queue)) {
					qe = dequeue(&tx_queue);
					sprintf_ts(msg,qe);
				} else {
					strcpy(msg,queue_empty);
				}
			}
			
			kfree(qe);
	} else {
		strcpy(msg,error_string);
	}

	/* Send the msg from kernel to the user */
	skb_out = nlmsg_new(msg_size, 0);
	if (!skb_out) {
		pr_err("Failed to allocate new skb\n");
		return;
	}

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);  
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	strncpy(nlmsg_data(nlh), msg, msg_size);

	res = nlmsg_unicast(nl_sk, skb_out, pid);
	if (res)
		pr_err("Error while sending back to user\n");
}

static int __init netlink_module_init(void) 
{
	struct netlink_kernel_cfg cfg = {
		.input = netlink_f_recv_msg,
	};
	
	getnstimeofday(&ts_current);
	
	pr_info("%s: Initializing... \n",__func__);
	myprintk_ts(&ts_current);

	nl_sk = 
		netlink_kernel_create(&init_net, 
			NETLINK_CUSTOM_PROTO, &cfg);
	if(!nl_sk)
	{
		pr_err("%s: Error creating netlink socket.\n", 
			__func__);
		return -1;

	}
	
	INIT_LIST_HEAD(&rx_queue);
	INIT_LIST_HEAD(&tx_queue);
	
	mod_timer(&mytimer, jiffies+TIMER_PERIOD);

	return 0;
}

static void __exit netlink_module_exit(void) 
{	
	getnstimeofday(&ts_current);
	pr_info("%s: Exiting... \n",__func__);
	myprintk_ts(&ts_current);
	del_timer(&mytimer);
	netlink_kernel_release(nl_sk);
	
	kfree_queue(&rx_queue);
	kfree_queue(&tx_queue);
}

module_init(netlink_module_init); 
module_exit(netlink_module_exit);

MODULE_LICENSE("GPL");
