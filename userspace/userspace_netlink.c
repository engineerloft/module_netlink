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

#include "nl_ts_queue.h"

#define NTIMES 100

struct nl_ts_socket {
	struct nl_sock *nlsock;
	int family_id;
	char ifname[IFNAME_SIZE];
};

void printf_ts(struct nl_ts *ts)
{
	printf("============== TS ================ \n");
	printf("Type: %s \n", (ts->type == 0) ? "Tx" : "Rx");
	printf("Sec: %lu \n", ts->sec);
	printf("Nsec: %lu \n", ts->nsec);
	printf("Seq: %lu \n", ts->seq);
	printf("ID: %u \n", ts->id);
	printf("Ahead: %d \n", ts->ahead);
	printf("Valid: %d \n", ts->valid);
	printf("================================== \n");
}

static struct nla_policy nested_policy[NL_TS_A_TS_NESTED_MAX + 1] = 
{
	[NL_TS_A_TS_NESTED_SEC] = { .type = NLA_U64 },
	[NL_TS_A_TS_NESTED_NSEC] = { .type = NLA_U64 },
	[NL_TS_A_TS_NESTED_SEQ] = { .type = NLA_U64 },
	[NL_TS_A_TS_NESTED_ID] = { .type = NLA_U16 },
	[NL_TS_A_TS_NESTED_AHEAD] = { .type = NLA_U32 },
	[NL_TS_A_TS_NESTED_VALID] = { .type = NLA_U32 },
	[NL_TS_A_TS_NESTED_TYPE] = { .type = NLA_U32 },
};

static int callback(struct nl_msg *msg, void *arg) {
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct nlattr *attr[NL_TS_A_MAX+1];
    struct nlattr *nested[NL_TS_A_TS_NESTED_MAX+1];
    char *buf;
    int err;
    struct nl_ts ts;
    
    err = genlmsg_parse(nlh, 0, attr, NL_TS_A_MAX, NULL);
    if(err != 0)
		printf("ERROR %d: Unable to parse NL attributes \n", err);
		
	nla_parse_nested(nested,NL_TS_A_TS_NESTED_MAX,
		attr[NL_TS_A_TS_NESTED],nested_policy);
		
	ts.sec = nla_get_u64(nested[NL_TS_A_TS_NESTED_SEC]);
	ts.nsec = nla_get_u64(nested[NL_TS_A_TS_NESTED_NSEC]);
	ts.seq = nla_get_u64(nested[NL_TS_A_TS_NESTED_SEQ]);
	ts.valid = nla_get_u32(nested[NL_TS_A_TS_NESTED_VALID]);
	ts.type = nla_get_u32(nested[NL_TS_A_TS_NESTED_TYPE]);
	ts.ahead = nla_get_u32(nested[NL_TS_A_TS_NESTED_AHEAD]);
	ts.id = nla_get_u16(nested[NL_TS_A_TS_NESTED_ID]);
    
    if (ts.type != MYNL_CMD_QEMPTY_RESP 
		&& ts.type != MYNL_CMD_QERROR_RESP) {
		printf_ts(&ts);
	} else {
		if (ts.type == MYNL_CMD_QEMPTY_RESP)
			printf("QUEUE EMPTY.\n");
		else
			printf("QUEUE ERROR.\n");
	}

    return NL_OK;
}

struct nl_ts_socket * nl_ts_socket_init(const char *ifname)
{
	struct nl_ts_socket *sock = NULL;
	int err;
	
	sock = malloc(sizeof(*sock));
	if(!sock)
		goto out2;
		
	strncpy(sock->ifname,ifname,IFNAME_SIZE);
	
	sock->nlsock = nl_socket_alloc();
	if(!sock->nlsock) {
		perror("ERROR: Unable to create socket \n");
		goto out2;
	}
	
	if(genl_connect(sock->nlsock) < 0) {
		perror("ERROR: Unable to connect to Generic Netlink \n");
		goto out1;
	}
	
	sock->family_id = genl_ctrl_resolve(sock->nlsock,"NL_TS_FAMILY");
	if(sock->family_id < 0) {
		perror("ERROR: Unable to get the Family ID \n");
		goto out1;
	}
	
	nl_socket_disable_seq_check(sock->nlsock);
	
	if((err = nl_socket_modify_cb(sock->nlsock, NL_CB_VALID, 
		NL_CB_CUSTOM, callback, NULL)) <  0 ) {
		printf("ERROR: Unable to modify valid message callback \n");
		goto out1;
	}
	
	return sock;

out1:
	nl_socket_free(sock->nlsock);
out2:
	return NULL;
}

int nl_socket_ts_ask(struct nl_ts_socket * sock, 
	int tx_rx)
{
	struct nl_msg *msg;
	void *p;
	int err;
	struct nlattr *nested;
	
	if(!sock)
		return -1;
	
	msg = nlmsg_alloc();
	if(!msg) {
		printf("ERROR: Unable to reserve memory \n");
		goto out2;
	}
		
	p = genlmsg_put(msg,0,0,sock->family_id,0,0,
		NL_TS_C_GETTS,VERSION_NR);
	if(!p) {
		printf("ERROR: Unable to initialize the header packet \n");
		goto out1;
	}
	
	nested = nla_nest_start(msg,NL_TS_A_TS_NESTED);
		
	if((err = nla_put_u32(msg,
		NL_TS_A_CMD_NESTED_CMD,tx_rx)) < 0) {
		printf("ERROR %d: Unable to add type nested attribute. \n",
			err);
		nla_nest_cancel(msg,nested);
		goto out1;
	}
		
	if((err = nla_put_string(msg,
		NL_TS_A_CMD_NESTED_IFACE,"iface0")) < 0) {
		printf("ERROR %d: Unable to add type nested attribute. \n",
			err);
		nla_nest_cancel(msg,nested);
		goto out1;
	}
		
	nla_nest_end(msg,nested);
	
	if((err = nl_send_auto_complete(sock->nlsock,msg)) < 0) {
		printf("ERROR: Unable to send the msg \n");
		goto out1;
	}
	
	if((err = nl_recvmsgs_default(sock->nlsock)) < 0) {
		printf("ERROR %d: Unable to receive the msg \n",err);
		goto out1;
	}
	
	nl_wait_for_ack(sock->nlsock);
	
	nlmsg_free(msg);
	return 0;

out1:
	nlmsg_free(msg);
out2:
	return -1;
}

void nl_ts_socket_free(struct nl_ts_socket *sock)
{
	if(!sock)
		return;
	
	nl_socket_free(sock->nlsock);
	free(sock);
}

int main(int argc, char *argv[]) {

	struct nl_ts_socket *sock;
	int i, ntimes;
    uint32_t tx_rx;
	
	if (argc < 2) {
		printf("Using %d times for the netlink test \n", NTIMES);
		ntimes = NTIMES;
	}
	else {
		ntimes = strtol(argv[1],(char **) NULL, 10);
	}
	
	sock = nl_ts_socket_init("iface0");
	if(!sock)
		goto out2;
	
	for(i = 0 ; i < ntimes ; i++) {
		if (i % 2 == 0)
			tx_rx = 0;
		else
			tx_rx = 1;
			
		if(nl_socket_ts_ask(sock, tx_rx) < 0)
			goto out1;
			
		//sleep(1);
	}
	
out1:
	nl_ts_socket_free(sock);
out2:
	return 0;
}
