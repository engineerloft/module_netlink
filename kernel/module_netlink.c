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

#include "myqueue.h"
  
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

static struct myqueue rx_queue;
static struct myqueue tx_queue;

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

#define MSG_SIZE_MAX 512
static void netlink_f_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int pid;
	struct sk_buff *skb_out;
	int res;
	int msg_size;
	struct mynl_cmd *cmd = NULL;
	int rx_queue_cmd = 0;
	int tx_queue_cmd = 0;
	int queue_cmd = 0;

	msg_size = MSG_SIZE_MAX;
	nlh = (struct nlmsghdr*)skb->data;
	pid = nlh->nlmsg_pid; /*pid of sending process */
	
	cmd = (struct mynl_cmd *) nlmsg_data(nlh);
	
	rx_queue_cmd = (cmd->cmd == MYNL_CMD_GETTS_RX);
	tx_queue_cmd = (cmd->cmd == MYNL_CMD_GETTS_TX);
	queue_cmd = rx_queue_cmd || tx_queue_cmd;
	
	if (queue_cmd) {
			struct myqueue_element *qe = NULL;
			
			if(rx_queue_cmd) {
				if (!is_queue_empty(&rx_queue)) {
					qe = dequeue(&rx_queue);
					cmd->ts = qe->ts;
					cmd->cmd = MYNL_CMD_OK_RESP;
				} else {
					cmd->cmd = MYNL_CMD_QEMPTY_RESP;
				}
			}
			else {
				if (!is_queue_empty(&tx_queue)) {
					qe = dequeue(&tx_queue);
					cmd->ts = qe->ts;
					cmd->cmd = MYNL_CMD_OK_RESP;
				} else {
					cmd->cmd = MYNL_CMD_QEMPTY_RESP;
				}
			}
			
			kfree(qe);
	} else {
		cmd->cmd = MYNL_CMD_QERROR_RESP;
	}

	/* Send the msg from kernel to the user */
	skb_out = nlmsg_new(msg_size, 0);
	if (!skb_out) {
		pr_err("Failed to allocate new skb\n");
		return;
	}

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);  
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	memcpy(nlmsg_data(nlh), cmd, msg_size);

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
	
	queue_init(&rx_queue);
	queue_init(&tx_queue);
	
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
