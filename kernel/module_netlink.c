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

#include "myqueue.h"

//Code based on 
//http://people.ee.ethz.ch/~arkeller/linux/multi/kernel_user_space_howto-3.html

#define TIMER_PERIOD 1*HZ
#define TX_TS_PERIOD 5
#define RX_TS_PERIOD 5
static int mytimer_events = 0;
static u64 tx_seq = 0;
static u64 rx_seq = 0;
static void mytimer_handler(unsigned long data);
DEFINE_TIMER(mytimer, mytimer_handler, 0, 0);
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
		//myprintk_ts(&ts_current);

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

/* attributes (variables):
 * the index in this enum is used as a reference for the type,
 * userspace application has to indicate the corresponding type
 * the policy is used for security considerations 
 */
enum {
	DOC_EXMPL_A_UNSPEC,
	DOC_EXMPL_A_MSG,
	__DOC_EXMPL_A_MAX,
};
#define DOC_EXMPL_A_MAX (__DOC_EXMPL_A_MAX - 1)

/* attribute policy: defines which attribute has which type (e.g int, char * etc)
 * possible values defined in net/netlink.h 
 */
static struct nla_policy doc_exmpl_genl_policy[DOC_EXMPL_A_MAX + 1] = {
	[DOC_EXMPL_A_MSG] = { .type = NLA_NUL_STRING },
};

#define VERSION_NR 1
//family definition
static struct genl_family doc_exmpl_gnl_family = {
	//.id = GENL_ID_GENERATE,         //Genetlink should generate an id
	.hdrsize = 0,
	.name = "HA_TS_FAMILY",        //The name of this family, used by userspace application
	.version = VERSION_NR,          //Version number  
	.maxattr = DOC_EXMPL_A_MAX,
};

/* commands: enumeration of all commands (functions), 
 * used by userspace application to identify command to be executed
 */
enum {
	DOC_EXMPL_C_UNSPEC,
	DOC_EXMPL_C_GETTS,
	__DOC_EXMPL_C_MAX,
};
#define DOC_EXMPL_C_MAX (__DOC_EXMPL_C_MAX - 1)

#define MSG_SIZE_MAX 512
int doc_exmpl_getts(struct sk_buff *skb_2, struct genl_info *info) {
	struct nlattr *na;
	struct sk_buff *skb;
	int rc;
	void *msg_head;
	char * mydata;
	int msg_size;
	char msg[MSG_SIZE_MAX];
	char *error_string = "ERROR";
	char *queue_empty = "QUEUE EMPTY";
	int rx_queue_cmd = 0;
	int tx_queue_cmd = 0;
	int queue_cmd = 0;
	
	msg_size = MSG_SIZE_MAX;
	
	if (info == NULL) {
		goto out;
	}
	
	/* For each attribute there is an index in info->attrs which points to a nlattr structure
	 * in this structure the data is given
	 */
	 na = info->attrs[DOC_EXMPL_A_MSG];
	 if (na) {
		mydata = (char *)nla_data(na);
		if (mydata == NULL) {
			 printk("error while receiving data\n");
			 goto out;
		} else {
			printk("received: %s\n", mydata);
		}
	} else {
		printk("no info->attrs %i\n", DOC_EXMPL_A_MSG);
		goto out;
	}
	
	rx_queue_cmd = !strcmp("GETTS_RX",mydata);
	tx_queue_cmd = !strcmp("GETTS_TX",mydata);
	queue_cmd = rx_queue_cmd || tx_queue_cmd;
	
	if (queue_cmd) {
			struct myqueue_element *qe = NULL;
			
			if(rx_queue_cmd) {
				if (!is_queue_empty(&rx_queue)) {
					qe = dequeue(&rx_queue);
					sprintf_ts(msg,qe);
				} else {
					strcpy(msg,queue_empty);
				}
			}
			else {
				if (!is_queue_empty(&tx_queue)) {
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
	
	//Send a message back
	//TRICK: If you dont know the size, use NLMSG_GOODSIZE
	skb = genlmsg_new(msg_size, GFP_KERNEL);
	if (skb == NULL) {
		goto out;
	}
	
	//Create the message headers
	/* arguments of genlmsg_put: 
	 * struct sk_buff *, 
	 * int (sending) pid, 
	 * int sequence number, 
	 * struct genl_family *, 
	 * int flags, 
	 * u8 command index (why do we need this?)
	 * */
	 
	msg_head = genlmsg_put(skb, 0, info->snd_seq+1, 
		&doc_exmpl_gnl_family, 0, DOC_EXMPL_C_GETTS);
	if (msg_head == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	
	//Add a DOC_EXMPL_A_MSG attribute (actual value to be sent)
	rc = nla_put_string(skb, DOC_EXMPL_A_MSG, msg);
	if (rc != 0) {
		goto out;
	}
	
	//Finalize the message
	genlmsg_end(skb, msg_head);
	
	//Send the message back
	rc = genlmsg_unicast(genl_info_net(info), skb,info->snd_portid );
	if (rc != 0) {
		goto out;
	}
	return 0;
out:
	printk("An error occured in doc_exmpl_echo:\n");
	return 0;
}

//Commands: mapping between the command enumeration and the actual function
struct genl_ops doc_exmpl_gnl_ops_echo = {
	.cmd = DOC_EXMPL_C_GETTS,
	.flags = 0,
	.policy = doc_exmpl_genl_policy,
	.doit = doc_exmpl_getts,
	.dumpit = NULL,
};

static int __init module_netlink_init(void) {
	int rc;
	struct genl_ops * ops = &doc_exmpl_gnl_ops_echo;
	
	getnstimeofday(&ts_current);
	printk("Generic Netlink Example Module inserted.\n");
	myprintk_ts(&ts_current);
	
	// Fill the family ops
	doc_exmpl_gnl_family.ops = ops;
	doc_exmpl_gnl_family.n_ops = 1;
	doc_exmpl_gnl_family.mcgrps = NULL;
	doc_exmpl_gnl_family.n_mcgrps = 0;
	
	// Register the family
	rc = genl_register_family(&doc_exmpl_gnl_family);
	if (rc != 0) {
		goto failure;
	}
	
	queue_init(&rx_queue);
	queue_init(&tx_queue);
	
	mod_timer(&mytimer, jiffies+TIMER_PERIOD);

	return 0; 
failure:
	printk("An error occured while inserting the generic netlink example module\n");
	return -1;
}

static void __exit module_netlink_exit(void) {
	int ret;
	
	getnstimeofday(&ts_current);
	printk("Generic Netlink Example Module unloaded.\n");
	myprintk_ts(&ts_current);
	del_timer(&mytimer);
	
	//Unregister the family
	ret = genl_unregister_family(&doc_exmpl_gnl_family);
	if(ret !=0) {
		printk("Unregister family %i\n",ret);
	}
	
	kfree_queue(&rx_queue);
	kfree_queue(&tx_queue);
}

module_init(module_netlink_init);
module_exit(module_netlink_exit);
MODULE_LICENSE("GPL");
