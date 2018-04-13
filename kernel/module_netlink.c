#include <net/netlink.h>
#include <net/genetlink.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <net/sock.h>
#include <linux/skbuff.h>

#include "nl_ts_module.h"

#define TIMER_PERIOD 1*HZ
#define TX_TS_PERIOD 5
#define RX_TS_PERIOD 5
static int mytimer_events = 0;
static u64 tx_seq = 0;
static u64 rx_seq = 0;
static void mytimer_handler(unsigned long data);
DEFINE_TIMER(mytimer, mytimer_handler, 0, 0);
struct timespec ts_current;

static int nl_ts_desc;

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
		struct nl_ts tmp;
		struct timespec *ts = &ts_current;
		
		getnstimeofday(ts);
		
		tmp.sec = ts->tv_sec;
		tmp.nsec = ts->tv_nsec;
		tmp.valid = 1;
		tmp.ahead = 0;
		tmp.id = 0;

		if (mytimer_events % TX_TS_PERIOD == 0) {
			tmp.type = MYNL_CMD_TX_OK_RESP;
			tmp.seq = tx_seq;
			nl_ts_iface_tx_ts_add(nl_ts_desc,&tmp);
			tx_seq++;
		}
			
		if (mytimer_events % RX_TS_PERIOD == 0) {
			tmp.type = MYNL_CMD_RX_OK_RESP;
			tmp.seq = rx_seq;
			nl_ts_iface_rx_ts_add(nl_ts_desc,&tmp);
			rx_seq++;
		}
		
		mytimer_events++;

		mod_timer(&mytimer, jiffies + TIMER_PERIOD);
}

static int __init module_netlink_init(void) {
	
	const char *ifname = "iface0";
	
	getnstimeofday(&ts_current);
	myprintk_ts(&ts_current);
	
	nl_ts_desc = nl_ts_iface_register(ifname);
	if(nl_ts_desc < 0)
		goto failure;
		
	printk("Netlink TS iface: %s desc: %d \n", 
		ifname, nl_ts_desc);
	
	mod_timer(&mytimer, jiffies+TIMER_PERIOD);

	return 0; 
failure:
	return -1;
}

static void __exit module_netlink_exit(void) {
	
	getnstimeofday(&ts_current);
	myprintk_ts(&ts_current);
	del_timer(&mytimer);
	
	nl_ts_iface_unregister(nl_ts_desc);
}

module_init(module_netlink_init);
module_exit(module_netlink_exit);
MODULE_LICENSE("GPL");
