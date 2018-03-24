#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#include <linux/netlink.h>
#include <linux/genetlink.h>

#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/cli/utils.h>

#include "mnlattr.h"

#define NTIMES 100

struct myts {
		uint64_t sec;
		uint64_t nsec;
		uint64_t seq;
		int valid;
		int type;
};

void printf_myts(struct myts *ts)
{
	printf("============== TS ================ \n");
	printf("Sec: %lu \n", ts->sec);
	printf("Nsec: %lu \n", ts->nsec);
	printf("Valid: %d \n", ts->valid);
	printf("TX_RX: %s \n", (ts->type == 0) ? "Tx" : "Rx");
	printf("Seq: %lu \n", ts->seq);
	printf("================================== \n");
}

static struct nla_policy nested_policy[DOC_EXMPL_A_TS_NESTED_MAX + 1] = 
{
	[DOC_EXMPL_A_TS_NESTED_SEC] = { .type = NLA_U64 },
	[DOC_EXMPL_A_TS_NESTED_NSEC] = { .type = NLA_U64 },
	[DOC_EXMPL_A_TS_NESTED_SEQ] = { .type = NLA_U64 },
	[DOC_EXMPL_A_TS_NESTED_VALID] = { .type = NLA_U32 },
	[DOC_EXMPL_A_TS_NESTED_TYPE] = { .type = NLA_U32 },
};

static int callback(struct nl_msg *msg, void *arg) {
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct nlattr *attr[DOC_EXMPL_A_MAX+1];
    struct nlattr *nested[DOC_EXMPL_A_TS_NESTED_MAX+1];
    char *buf;
    int err;
    struct myts ts;
    
    err = genlmsg_parse(nlh, 0, attr, DOC_EXMPL_A_MAX, NULL);
    if(err != 0)
		printf("ERROR %d: Unable to parse NL attributes \n", err);
		
	nla_parse_nested(nested,DOC_EXMPL_A_TS_NESTED_MAX,
		attr[DOC_EXMPL_A_TS_NESTED],nested_policy);
		
	ts.sec = nla_get_u64(nested[DOC_EXMPL_A_TS_NESTED_SEC]);
	ts.nsec = nla_get_u64(nested[DOC_EXMPL_A_TS_NESTED_NSEC]);
	ts.seq = nla_get_u64(nested[DOC_EXMPL_A_TS_NESTED_SEQ]);
	ts.valid = nla_get_u32(nested[DOC_EXMPL_A_TS_NESTED_VALID]);
	ts.type = nla_get_u32(nested[DOC_EXMPL_A_TS_NESTED_TYPE]);
    
    if (ts.type != 2 && ts.type != 3) {
		printf_myts(&ts);
	} else {
		if (ts.type == 2)
			printf("QUEUE EMPTY.\n");
		else
			printf("QUEUE ERROR.\n");
	}

    return NL_OK;
}

int main(int argc, char *argv[]) {

	struct nl_sock *nlsock;
	struct nl_msg *msg;
	int family_id;
	void *p;
	int err;
	int i, ntimes;
    uint32_t tx_rx;
	struct nlattr *nested;
	
	if (argc < 2) {
		printf("Using %d times for the netlink test \n", NTIMES);
		ntimes = NTIMES;
	}
	else {
		ntimes = strtol(argv[1],(char **) NULL, 10);
	}
	
	nlsock = nl_socket_alloc();
	if(!nlsock) {
		perror("Creating socket...\n");
		return -1;
	}
	
	genl_connect(nlsock);
	
	family_id = genl_ctrl_resolve(nlsock,"HA_TS_FAMILY");
	
	nl_socket_disable_seq_check(nlsock);
	
	if((err = nl_socket_modify_cb(nlsock, NL_CB_VALID, 
		NL_CB_CUSTOM, callback, NULL)) <  0 ) {
		printf("ERROR: Unable to modify valid message callback \n");
		goto out2;
	}
	
	for(i = 0 ; i < ntimes ; i++) {
		msg = nlmsg_alloc();
		if(!msg) {
			printf("ERROR: Unable to reserve memory \n");
			goto out2;
		}
		
		p = genlmsg_put(msg,0,0,family_id,0,0,1,1);
		if(!p) {
			printf("ERROR: Unable to initialize the header packet \n");
			goto out1;
		}
	
		if (i % 2 == 0)
			tx_rx = 0;
		else
			tx_rx = 1;
			
		nested = nla_nest_start(msg,DOC_EXMPL_A_TS_NESTED);
		
		if((err = nla_put_u32(msg,
			DOC_EXMPL_A_TS_NESTED_TYPE,tx_rx)) < 0) {
			printf("ERROR %d: Unable to add type nested attribute. \n",
				err);
			nla_nest_cancel(msg,nested);
			goto out1;
		}
		
		if((err = nla_put_u64(msg,
			DOC_EXMPL_A_TS_NESTED_SEC,0)) < 0) {
			printf("ERROR %d: Unable to add sec nested attribute. \n", 
				err);
			nla_nest_cancel(msg,nested);
			goto out1;
		}
		
		if((err = nla_put_u64(msg,
			DOC_EXMPL_A_TS_NESTED_NSEC,0)) < 0) {
			printf("ERROR %d: Unable to add nsec nested attribute. \n", 
				err);
			nla_nest_cancel(msg,nested);
			goto out1;
		}
		
		if((err = nla_put_u64(msg,
			DOC_EXMPL_A_TS_NESTED_SEQ,0)) < 0) {
			printf("ERROR %d: Unable to add seq nested attribute. \n", 
				err);
			nla_nest_cancel(msg,nested);
			goto out1;
		}
		
		if((err = nla_put_u32(msg,
			DOC_EXMPL_A_TS_NESTED_VALID,0)) < 0) {
			printf("ERROR %d: Unable to add valid nested attribute. \n", 
				err);
			nla_nest_cancel(msg,nested);
			goto out1;
		}
		
		nla_nest_end(msg,nested);
	
		if((err = nl_send_auto_complete(nlsock,msg)) < 0) {
			printf("ERROR: Unable to send the msg \n");
			goto out1;
		}
	
		if((err = nl_recvmsgs_default(nlsock)) < 0) {
			printf("ERROR %d: Unable to receive the msg \n",err);
			goto out1;
		}
	
		nl_wait_for_ack(nlsock);
		
		//sleep(1);
		
out1:
		nlmsg_free(msg);
	}
	
out2:
	nl_socket_free(nlsock);
	
	return 0;
}
