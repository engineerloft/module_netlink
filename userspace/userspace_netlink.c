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

#define NTIMES 100

struct myts {
		uint64_t hour;
		uint64_t min;
		uint64_t sec;
		uint64_t nsec;
		uint64_t seq;
		int valid;
		int tx_rx;
};

void printf_myts(struct myts *ts)
{
	printf("============== TS ================ \n");
	printf("Hour: %lu \n", ts->hour);
	printf("Min: %lu \n", ts->min);
	printf("Sec: %lu \n", ts->sec);
	printf("Nsec: %lu \n", ts->nsec);
	printf("Valid: %d \n", ts->valid);
	printf("TX_RX: %s \n", (ts->tx_rx == 0) ? "Tx" : "Rx");
	printf("Seq: %lu \n", ts->seq);
	printf("================================== \n");
}

static int callback(struct nl_msg *msg, void *arg) {
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct nlattr *attr[2];
    char *buf;
    int err;
    struct myts ts;
    
    err = genlmsg_parse(nlh, 0, attr, 1, NULL);
    if(err != 0)
		printf("ERROR %d: Unable to parse NL attributes \n", err);
		
	buf = nla_get_string(attr[1]);
    
    if (strcmp(buf,"ERROR") && strcmp(buf,"QUEUE EMPTY")) {
		sscanf(buf, "%lu:%lu:%lu:%lu:v%d:t%d:s%lu",
			&ts.hour, &ts.min, &ts.sec, 
			&ts.nsec, &ts.valid, 
			&ts.tx_rx, &ts.seq);
		printf_myts(&ts);
	} else {
		printf("%s\n",buf);
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
	char msg_buf[100];
	
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
	
	for(i = 0 ; i < ntimes ; i++) {
		if (i % 2 == 0)
			strcpy(msg_buf,"GETTS_TX");
		else
			strcpy(msg_buf,"GETTS_RX");
			
		if((err = nla_put_string(msg,1,msg_buf)) < 0) {
			printf("ERROR: Unable to add the string \n");
			goto out1;
		}
	
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
	}
	
out1:
	nlmsg_free(msg);
out2:
	nl_socket_free(nlsock);
	
	return 0;
}
