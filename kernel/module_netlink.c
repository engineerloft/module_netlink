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
static void mytimer_handler(unsigned long data);
DEFINE_TIMER(mytimer, mytimer_handler, 0, 0);

struct sock *nl_sk = NULL;
struct timespec ts_current;

struct myts {
		u64 sec;
		u64 nsec;
		int valid;
};

struct myqueue_element {
		struct list_head next;
		struct myts ts;
};

static struct list_head queue;

static void myprintk_ts(struct timespec *ts)
{
	printk("%.2lu:%.2lu:%.2lu:%.6lu \n",
		(ts->tv_sec / 3600) % (24),
		(ts->tv_sec / 60) % (60),
		ts->tv_sec % 60,
		ts->tv_nsec / 1000);
}

static void mytimer_handler(unsigned long data)
{
		struct myqueue_element *new_ts;

		getnstimeofday(&ts_current);

		new_ts = kmalloc(sizeof(struct myqueue_element),
			GFP_ATOMIC);

		(new_ts->ts).sec = ts_current.tv_sec;
		(new_ts->ts).nsec = ts_current.tv_nsec;
		(new_ts->ts).valid = 1;

		list_add_tail(&new_ts->next, &queue);

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

	msg_size = strlen(msg);
	nlh = (struct nlmsghdr*)skb->data;
	pid = nlh->nlmsg_pid; /*pid of sending process */
	
	recv_buf = (char *) nlmsg_data(nlh);
	
	if (!list_empty(&queue)) {
		if(!strcmp("GETTS",recv_buf)) {
			struct myqueue_element *qe;
			qe = list_first_entry(&queue, struct myqueue_element,
			next);
			list_del(&qe->next);
			
			sprintf(msg,"%.2llu:%.2llu:%.2llu:%.6llu:v%d",
				(qe->ts.sec / 3600) % (24),
				(qe->ts.sec / 60) % (60),
				qe->ts.sec % 60,
				qe->ts.nsec / 1000,
				qe->ts.valid);
			
			kfree(qe);
		} else {
			strcpy(msg,error_string);
		}
	} else {
		strcpy(msg,queue_empty);
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
	
	INIT_LIST_HEAD(&queue);
	
	mod_timer(&mytimer, jiffies+TIMER_PERIOD);

	return 0;
}

static void __exit netlink_module_exit(void) 
{
	struct myqueue_element *qe;
	
	getnstimeofday(&ts_current);
	pr_info("%s: Exiting... \n",__func__);
	myprintk_ts(&ts_current);
	del_timer(&mytimer);
	netlink_kernel_release(nl_sk);
	
	while (!list_empty(&queue)) {
		qe = list_first_entry(&queue, struct myqueue_element,
			next);
		list_del(&qe->next);
		kfree(qe);
	}
}

module_init(netlink_module_init); 
module_exit(netlink_module_exit);

MODULE_LICENSE("GPL");
