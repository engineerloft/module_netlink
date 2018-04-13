#ifndef __NL_TS_QUEUE__
#define __NL_TS_QUEUE__

#ifdef __KERNEL__
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#endif

#define IFNAME_SIZE 10
#define VERSION_NR 1

enum {
	NL_TS_A_UNSPEC,
	NL_TS_A_TS_NESTED,
	__NL_TS_A_MAX,
};
#define NL_TS_A_MAX (__NL_TS_A_MAX - 1)

enum {
	NL_TS_A_TS_NESTED_UNSPEC,
	NL_TS_A_TS_NESTED_TYPE,
	NL_TS_A_TS_NESTED_SEC,
	NL_TS_A_TS_NESTED_NSEC,
	NL_TS_A_TS_NESTED_SEQ,
	NL_TS_A_TS_NESTED_ID,
	NL_TS_A_TS_NESTED_AHEAD,
	NL_TS_A_TS_NESTED_VALID,
	__NL_TS_A_TS_NESTED_MAX,
};
#define NL_TS_A_TS_NESTED_MAX (__NL_TS_A_TS_NESTED_MAX - 1)

struct nl_ts {
		int type;
#ifdef __KERNEL__
		u64 sec;
#else
		uint64_t sec;
#endif
#ifdef __KERNEL__
		u64 nsec;
#else
		uint64_t nsec;
#endif
#ifdef __KERNEL__
		u64 seq;
#else
		uint64_t seq;
#endif
#ifdef __KERNEL__
		u16 id;
#else
		uint16_t id;
#endif
		int ahead;
		int valid;
};

#define MYNL_CMD_GETTS_TX 0
#define MYNL_CMD_GETTS_RX 1
#define MYNL_CMD_OK_RESP 2
#define MYNL_CMD_QEMPTY_RESP 3
#define MYNL_CMD_QERROR_RESP 4

enum {
	NL_TS_A_CMD_NESTED_UNSPEC,
	NL_TS_A_CMD_NESTED_CMD,
	NL_TS_A_CMD_NESTED_IFACE,
	__NL_TS_A_CMD_NESTED_MAX,
};
#define NL_TS_A_CMD_NESTED_MAX (__NL_TS_A_CMD_NESTED_MAX - 1)

enum {
	NL_TS_C_UNSPEC,
	NL_TS_C_GETTS,
	__NL_TS_C_MAX,
};
#define NL_TS_C_MAX (__NL_TS_C_MAX - 1)

struct nl_ts_cmd {
	int cmd;
	char iface[IFNAME_SIZE];
};

#ifdef __KERNEL__
struct nl_ts_queue_element {
		struct list_head next;
		struct nl_ts ts;
};

struct nl_ts_queue {
	struct list_head queue;
	spinlock_t lock;
};

struct nl_ts_queue_element * nl_ts_queue_kmalloc(struct nl_ts *ts);
void nl_ts_queue_init(struct nl_ts_queue *q);
int nl_ts_queue_enqueue(struct nl_ts_queue *q, 
	struct nl_ts_queue_element *qe);
struct nl_ts_queue_element *nl_ts_queue_dequeue(struct nl_ts_queue *q);
int nl_ts_queue_is_empty(struct nl_ts_queue *q);
void nl_ts_queue_kfree(struct nl_ts_queue *q);
void nl_ts_queue_printk(struct nl_ts_queue *q);
#endif

#endif /* __NL_TS_QUEUE__ */
