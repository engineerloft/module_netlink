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

#include "nl_ts_queue.h"

#define N_NL_TS_SLOTS 256

#define IFNAME_SIZE 10
struct nl_ts_table_entry {
	struct nl_ts_queue rx_queue;
	struct nl_ts_queue tx_queue;
	int assigned;
	char ifname[IFNAME_SIZE];
	spinlock_t lock;
};

struct nl_ts_table {
	struct nl_ts_table_entry _nl_ts_table[N_NL_TS_SLOTS];
};

static struct nl_ts_table nl_ts_tbl;

/* attribute policy: defines which attribute has which type (e.g int, char * etc)
 * possible values defined in net/netlink.h 
 */
static struct nla_policy nl_ts_genl_policy[NL_TS_A_MAX + 1] = {
	[NL_TS_A_TS_NESTED] = { .type = NLA_NESTED },
};

static struct nla_policy nl_ts_genl_cmd_nested_policy[NL_TS_A_CMD_NESTED_MAX + 1] = {
	[NL_TS_A_CMD_NESTED_CMD] = { .type = NLA_U32 },
	[NL_TS_A_CMD_NESTED_IFACE] = { .type = NLA_NUL_STRING, .len = IFNAME_SIZE-1 },
};

#define VERSION_NR 1
//family definition
static struct genl_family nl_ts_gnl_family = {
	//.id = GENL_ID_GENERATE,         //Genetlink should generate an id
	.hdrsize = 0,
	.name = "NL_TS_FAMILY",        //The name of this family, used by userspace application
	.version = VERSION_NR,          //Version number  
	.maxattr = NL_TS_A_MAX,
};

/* commands: enumeration of all commands (functions), 
 * used by userspace application to identify command to be executed
 */
enum {
	NL_TS_C_UNSPEC,
	NL_TS_C_GETTS,
	__NL_TS_C_MAX,
};
#define NL_TS_C_MAX (__NL_TS_C_MAX - 1)

static struct nl_ts_table_entry * nl_ts_table_entry_get(int desc)
{
	if (desc < 0 || desc > N_NL_TS_SLOTS)
		return NULL;
	else
		return  &(nl_ts_tbl._nl_ts_table[desc]);
}

static int nl_ts_table_entry_get_by_ifname(char *ifname)
{
	int desc = -1;
	struct nl_ts_table_entry *tmp = NULL;
	int i;
	
	for(i = 0 ; i < N_NL_TS_SLOTS ; i++) {
		tmp = &(nl_ts_tbl._nl_ts_table[i]);
		if(!strcmp(tmp->ifname, ifname)) {
			desc = i;
			break;
		}
	}
	
	return desc;
}

static int nl_ts_parse_skb(struct sk_buff *skb, 
	struct genl_info *info, struct nl_ts_cmd *cmd)
{
	int rc;
	u32 cmd_code;
	char *iface;
	int iface_desc = -1;
	struct nlattr *na;
	struct nlattr *nested[NL_TS_A_CMD_NESTED_MAX+1];
	
	if (info == NULL) {
		cmd->cmd = MYNL_CMD_QERROR_RESP;
	} else {
		na = info->attrs[NL_TS_A_TS_NESTED];
		rc = nla_parse_nested(nested, 
			NL_TS_A_CMD_NESTED_MAX, na, 
			nl_ts_genl_cmd_nested_policy);
	
		if(!nested[NL_TS_A_CMD_NESTED_CMD] || 
			!nested[NL_TS_A_CMD_NESTED_IFACE]) {
			cmd->cmd = MYNL_CMD_QERROR_RESP;
		} else {
			na = nested[NL_TS_A_CMD_NESTED_CMD];
			cmd_code = nla_get_u32(na);
			
			cmd->cmd = cmd_code;
			
			na = nested[NL_TS_A_CMD_NESTED_IFACE];
			nla_strlcpy(iface,na,IFNAME_SIZE);
			
			iface_desc = nl_ts_table_entry_get_by_ifname(iface);
			
			if (iface_desc < 0 || iface_desc >= N_NL_TS_SLOTS) {
				cmd->cmd = MYNL_CMD_QERROR_RESP;
			} else {
				cmd->iface = iface;
			}
		}
	}
	
	return iface_desc;
}

static int nl_ts_userland_send(struct nl_ts *ts, 
	struct genl_info *info)
{
	struct sk_buff *skb;
	int rc = 0;
	void *msg_head;
	struct nlattr *na;
	
	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (skb == NULL) {
		rc = -1;
		goto out;
	}
	 
	msg_head = genlmsg_put(skb, 0, info->snd_seq+1, 
		&nl_ts_gnl_family, 0, NL_TS_C_GETTS);
	if (msg_head == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	
	na = nla_nest_start(skb, 
		NL_TS_A_TS_NESTED);
	if(!na) {
		rc = -1;
		goto out;
	}
	
	rc = nla_put_u32(skb,NL_TS_A_TS_NESTED_TYPE,
		(u32) ts->type);
	if (rc != 0) {
		nla_nest_cancel(skb,na);
		rc = -1;
		goto out;
	}
	
	rc = nla_put_u64(skb,NL_TS_A_TS_NESTED_SEC,
		(u64) ts->sec);
	if (rc != 0) {
		nla_nest_cancel(skb,na);
		rc = -1;
		goto out;
	}
	
	rc = nla_put_u64(skb,NL_TS_A_TS_NESTED_NSEC,
		(u64) ts->nsec);
	if (rc != 0) {
		nla_nest_cancel(skb,na);
		rc = -1;
		goto out;
	}
	
	rc = nla_put_u64(skb,NL_TS_A_TS_NESTED_SEQ,
		(u64) ts->seq);
	if (rc != 0) {
		nla_nest_cancel(skb,na);
		rc = -1;
		goto out;
	}
	
	rc = nla_put_u16(skb,NL_TS_A_TS_NESTED_ID,
		(u16) ts->id);
	if (rc != 0) {
		nla_nest_cancel(skb,na);
		rc = -1;
		goto out;
	}
	
	rc = nla_put_u32(skb,NL_TS_A_TS_NESTED_AHEAD,
		(u32) ts->ahead);
	if (rc != 0) {
		nla_nest_cancel(skb,na);
		rc = -1;
		goto out;
	}
	
	rc = nla_put_u32(skb,NL_TS_A_TS_NESTED_VALID,
		(u32) ts->valid);
	if (rc != 0) {
		nla_nest_cancel(skb,na);
		rc = -1;
		goto out;
	}
	
	nla_nest_end(skb, na);
	
	genlmsg_end(skb, msg_head);
	
	rc = genlmsg_unicast(genl_info_net(info), skb,info->snd_portid );
	if (rc != 0) {
		rc = -1;
		goto out;
	}
	
	return rc;

out:
	return rc;
}

int nl_ts_getts(struct sk_buff *skb, struct genl_info *info) {
	int rx_queue_cmd = 0;
	int tx_queue_cmd = 0;
	int queue_cmd = 0;
	int iface_desc;
	struct nl_ts ts;
	struct nl_ts_cmd cmd;
	struct nl_ts_queue *tx_q = NULL;
	struct nl_ts_queue *rx_q = NULL;
	struct nl_ts_queue_element *qe = NULL;
	struct nl_ts_table_entry * tbl_entry = NULL;
	
	if (info == NULL) {
		goto out;
	}
	
	memset((void *) &cmd, 0, sizeof(cmd));

	iface_desc = nl_ts_parse_skb(skb,info,&cmd);
	
	kfree_skb(skb);
	
	ts.sec = 0;
	ts.nsec = 0;
	ts.seq = 0;
	ts.valid = 0;
	ts.ahead = 0;
	ts.id = 0;
	ts.type = 3;
	
	rx_queue_cmd = (cmd.cmd == 1);
	tx_queue_cmd = (cmd.cmd == 0);
	queue_cmd = rx_queue_cmd || tx_queue_cmd;
	
	tbl_entry = nl_ts_table_entry_get(iface_desc);
	
	if(!tbl_entry || !tbl_entry->assigned)
		queue_cmd = 0;
	
	if (queue_cmd) {
		if(rx_queue_cmd) {
			rx_q = &(tbl_entry->rx_queue);
			
			qe = nl_ts_queue_dequeue(rx_q);
			if(qe) {
				ts = qe->ts;
			} else {
				ts.type = 2;
			}
		}
		else {
			tx_q = &(tbl_entry->tx_queue);
			
			qe = nl_ts_queue_dequeue(tx_q);
			if(qe) {
				ts = qe->ts;
			} else {
				ts.type = 2;
			}
		}
			
		kfree(qe);
		
		if(nl_ts_userland_send(&ts,info))
			goto out;
	}
	
	return 0;
out:
	printk("An error occured in generic netlink callback. \n");
	return 0;
}

struct genl_ops nl_ts_gnl_ops[NL_TS_C_MAX+1] = {
		[NL_TS_C_GETTS] = {
			.cmd = NL_TS_C_GETTS,
			.flags = 0,
			.policy = nl_ts_genl_policy,
			.doit = nl_ts_getts,
			.dumpit = NULL,
		},
};

int nl_ts_iface_tx_ts_add(int iface_desc, struct nl_ts *ts)
{
	struct nl_ts_table_entry * tbl_entry = NULL;
	struct nl_ts_queue_element * ts_q_elem = NULL;
	struct nl_ts_queue * ts_q = NULL;
	spinlock_t *sl  = NULL;
	unsigned long flags;
	
	if(!ts)
		return -1;
	
	tbl_entry = nl_ts_table_entry_get(iface_desc);
	if(!tbl_entry)
		return -1;
		
	sl = &(tbl_entry->lock);
	
	spin_lock_irqsave(sl, flags);
	ts_q_elem = nl_ts_queue_kmalloc(ts);
	ts_q = &(tbl_entry->tx_queue);
	if(ts_q_elem)
		nl_ts_queue_enqueue(ts_q,ts_q_elem);

	spin_unlock_irqrestore(sl, flags);
		
	return 0;
}
EXPORT_SYMBOL(nl_ts_iface_tx_ts_add);

int nl_ts_iface_rx_ts_add(int iface_desc, struct nl_ts *ts)
{
	struct nl_ts_table_entry * tbl_entry = NULL;
	struct nl_ts_queue_element * ts_q_elem = NULL;
	struct nl_ts_queue * ts_q = NULL;
	spinlock_t *sl  = NULL;
	unsigned long flags;
	
	if(!ts)
		return -1;
		
	tbl_entry = nl_ts_table_entry_get(iface_desc);
	if(!tbl_entry)
		return -1;
		
	sl = &(tbl_entry->lock);
	
	spin_lock_irqsave(sl, flags);
	ts_q_elem = nl_ts_queue_kmalloc(ts);
	ts_q = &(tbl_entry->rx_queue);
	if(ts_q_elem)
		nl_ts_queue_enqueue(ts_q,ts_q_elem);
		
	spin_unlock_irqrestore(sl, flags);
		
	return 0;
}
EXPORT_SYMBOL(nl_ts_iface_rx_ts_add);

int nl_ts_iface_register(const char *iface)
{
	unsigned long flags;
	spinlock_t *sl  = NULL;
	struct nl_ts_table_entry * tbl_entry  = NULL;
	int i;
	int desc = -1;
	
	for(i = 0 ; i < N_NL_TS_SLOTS ; i++) {
		tbl_entry = nl_ts_table_entry_get(i);
		sl = &(tbl_entry->lock);
		spin_lock_irqsave(sl, flags);
		if(tbl_entry->assigned == 0) {
			desc = i;
			strncpy(tbl_entry->ifname, iface, 10);
			tbl_entry->assigned = 1;
			nl_ts_queue_init(&tbl_entry->tx_queue);
			nl_ts_queue_init(&tbl_entry->rx_queue);
			spin_unlock_irqrestore(sl, flags);
			break;
		}
		spin_unlock_irqrestore(sl, flags);
	}
	
	return desc;
}
EXPORT_SYMBOL(nl_ts_iface_register);

int nl_ts_iface_unregister(int iface_desc)
{
	unsigned long flags;
	spinlock_t *sl  = NULL;
	struct nl_ts_table_entry * tbl_entry  = NULL;
	
	tbl_entry = nl_ts_table_entry_get(iface_desc);
	if(!tbl_entry)
		return -1;
		
	sl = &(tbl_entry->lock);
	
	spin_lock_irqsave(sl, flags);
	if(tbl_entry->assigned == 1) {
		tbl_entry->assigned = 0;
		strncpy(tbl_entry->ifname,
			"NULL",10);
		nl_ts_queue_kfree(&tbl_entry->tx_queue);
		nl_ts_queue_kfree(&tbl_entry->rx_queue);
	}
	spin_unlock_irqrestore(sl, flags);
	
	return 0;
}
EXPORT_SYMBOL(nl_ts_iface_unregister);

static int __init nl_ts_module_init(void) {
	int rc;
	struct genl_ops * ops = &nl_ts_gnl_ops[NL_TS_C_GETTS];
	struct nl_ts_table_entry * tbl_entry  = NULL;
	int i;
	
	for(i = 0 ; i < N_NL_TS_SLOTS ; i++) {
		tbl_entry = &(nl_ts_tbl._nl_ts_table[i]);
		tbl_entry->assigned = 0;
		strncpy(tbl_entry->ifname,
			"NULL",10);
		spin_lock_init(&(tbl_entry->lock));
	}
	
	// Fill the family ops
	nl_ts_gnl_family.ops = ops;
	nl_ts_gnl_family.n_ops = NL_TS_C_MAX;
	nl_ts_gnl_family.mcgrps = NULL;
	nl_ts_gnl_family.n_mcgrps = 0;
	
	// Register the family
	rc = genl_register_family(&nl_ts_gnl_family);
	if (rc != 0) {
		goto failure;
	}
	
	printk("Installed the Netlink TS family. \n");

	return 0; 
failure:
	printk("Error registering the Netlink TS family. \n");
	return -1;
}

static void __exit nl_ts_module_exit(void) {
	int ret;
	
	//Unregister the family
	ret = genl_unregister_family(&nl_ts_gnl_family);
	if(ret !=0) {
		printk("Error unregistering the Netlink TS family. \n");
	}
	
	printk("Removed the Netlink TS family. \n");
}

module_init(nl_ts_module_init);
module_exit(nl_ts_module_exit);
MODULE_LICENSE("GPL");
