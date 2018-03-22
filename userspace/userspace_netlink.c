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

#include <linux/genetlink.h>

//Code based on 
//http://people.ee.ethz.ch/~arkeller/linux/multi/kernel_user_space_howto-3.html

/* Generic macros for dealing with netlink sockets. Might be duplicated
 * elsewhere. It is recommended that commercial grade applications use
 * libnl or libnetlink and use the interfaces provided by the library
 */
#define GENLMSG_DATA(glh) ((void *)(NLMSG_DATA(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(glh) (NLMSG_PAYLOAD(glh, 0) - GENL_HDRLEN)
#define NLA_DATA(na) ((void *)((char*)(na) + NLA_HDRLEN))

#define NTIMES 100

//Variables used for netlink

//netlink socket's file descriptor
int nl_fd;

//netlink socket address
struct sockaddr_nl nl_address; 

//The family ID resolved by the netlink controller for 
// this userspace program
int nl_family_id;

//Number of bytes sent or received via send() or recv()
int nl_rxtx_length;

//pointer to netlink attributes structure within the payload
struct nlattr *nl_na; 

//memory for netlink request and response messages - headers are included
struct { 
	struct nlmsghdr n;
	struct genlmsghdr g;
	char buf[256];
} nl_request_msg, nl_response_msg;

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

int main(int argc, char *argv[]) {

	int ntimes;
	int i;
	char *buf;
	char cmd[512];
	struct myts ts;
	
	if (argc < 2) {
		printf("Using %d times for the netlink test \n", NTIMES);
		ntimes = NTIMES;
	}
	else {
		ntimes = strtol(argv[1],(char **) NULL, 10);
	}
	
	//Step 1: Open the socket. Note that protocol = NETLINK_GENERIC
	nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (nl_fd < 0) {
		perror("socket()");
		return -1;
	}
	
	//Step 2: Bind the socket.
	memset(&nl_address, 0, sizeof(nl_address));
	nl_address.nl_family = AF_NETLINK;
	nl_address.nl_groups = 0;
	
	if (bind(nl_fd, (struct sockaddr *) &nl_address, 
		sizeof(nl_address)) < 0) {
		perror("bind()");
		close(nl_fd);
		return -1;
	}
	
	//Step 3. Resolve the family ID corresponding to "CONTROL_EXMPL"
	//Populate the netlink header
	nl_request_msg.n.nlmsg_type = GENL_ID_CTRL;
	nl_request_msg.n.nlmsg_flags = NLM_F_REQUEST;
	nl_request_msg.n.nlmsg_seq = 0;
	nl_request_msg.n.nlmsg_pid = getpid();
	nl_request_msg.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	//Populate the payload's "family header" 
	nl_request_msg.g.cmd = CTRL_CMD_GETFAMILY;
	nl_request_msg.g.version = 0x1;
	//Populate the payload's "netlink attributes"
	nl_na = (struct nlattr *) GENLMSG_DATA(&nl_request_msg);
	nl_na->nla_type = CTRL_ATTR_FAMILY_NAME;
	nl_na->nla_len = strlen("HA_TS_FAMILY") + 1 + NLA_HDRLEN;
	//Family name length can be upto 16 chars including \0
	strcpy(NLA_DATA(nl_na), "HA_TS_FAMILY"); 
	
	nl_request_msg.n.nlmsg_len += NLMSG_ALIGN(nl_na->nla_len);
	
	memset(&nl_address, 0, sizeof(nl_address));
	nl_address.nl_family = AF_NETLINK;
	
	//Send the family ID request message to the netlink controller
	nl_rxtx_length = sendto(nl_fd, (char *) &nl_request_msg, 
		nl_request_msg.n.nlmsg_len, 0, (struct sockaddr *) &nl_address, 
		sizeof(nl_address));
	if (nl_rxtx_length != nl_request_msg.n.nlmsg_len) {
		perror("sendto()");
		close(nl_fd);
		return -1;
	}
	
	//Wait for the response message
	nl_rxtx_length = recv(nl_fd, &nl_response_msg, 
		sizeof(nl_response_msg), 0);
	if (nl_rxtx_length < 0) {
		perror("recv()");
		return -1;
	}
	
	//Validate response message
	if (!NLMSG_OK((&nl_response_msg.n), nl_rxtx_length)) {
		fprintf(stderr, "family ID request : invalid message\n");
		return -1;
	}
	if (nl_response_msg.n.nlmsg_type == NLMSG_ERROR) { //error
		fprintf(stderr, "family ID request : receive error\n");
		return -1;
	}
	
	//Extract family ID
	nl_na = (struct nlattr *) GENLMSG_DATA(&nl_response_msg);
	nl_na = 
		(struct nlattr *) ((char *) nl_na + NLA_ALIGN(nl_na->nla_len));
	if (nl_na->nla_type == CTRL_ATTR_FAMILY_ID) {
		nl_family_id = *(__u16 *) NLA_DATA(nl_na);
	}
	
	//Step 4. Send own custom message
	for(i = 0 ; i < ntimes ; i++) {
		if (i % 2 == 0)
			strcpy(cmd,"GETTS_TX");
		else
			strcpy(cmd,"GETTS_RX");
			
		memset(&nl_request_msg, 0, sizeof(nl_request_msg));
		memset(&nl_response_msg, 0, sizeof(nl_response_msg));
	
		nl_request_msg.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
		nl_request_msg.n.nlmsg_type = nl_family_id;
		nl_request_msg.n.nlmsg_flags = NLM_F_REQUEST;
		nl_request_msg.n.nlmsg_seq = 60;
		nl_request_msg.n.nlmsg_pid = getpid();
		nl_request_msg.g.cmd = 1; //corresponds to DOC_EXMPL_C_ECHO;
	
		nl_na = (struct nlattr *) GENLMSG_DATA(&nl_request_msg);
		nl_na->nla_type = 1; // corresponds to DOC_EXMPL_A_MSG
		nl_na->nla_len = sizeof(cmd)+NLA_HDRLEN; //Message length
		memcpy(NLA_DATA(nl_na), cmd, sizeof(cmd));
		nl_request_msg.n.nlmsg_len += NLMSG_ALIGN(nl_na->nla_len);
	
		memset(&nl_address, 0, sizeof(nl_address));
		nl_address.nl_family = AF_NETLINK;
	
		//Send the custom message
		nl_rxtx_length = sendto(nl_fd, (char *) &nl_request_msg, 
			nl_request_msg.n.nlmsg_len, 0, 
			(struct sockaddr *) &nl_address, sizeof(nl_address));
		if (nl_rxtx_length != nl_request_msg.n.nlmsg_len) {
			perror("sendto()");
			close(nl_fd);
			return -1;
		}
		printf("Sent to kernel: %s\n",cmd);
	
		//Receive reply from kernel
		nl_rxtx_length = recv(nl_fd, &nl_response_msg, 
			sizeof(nl_response_msg), 0);
		if (nl_rxtx_length < 0) {
			perror("recv()");
			return -1;
		}
	
		//Validate response message
		if (nl_response_msg.n.nlmsg_type == NLMSG_ERROR) { //Error
			printf("Error while receiving reply from kernel: NACK Received\n");
			close(nl_fd);
			return -1;
		}
	
		if (nl_rxtx_length < 0) {
			printf("Error while receiving reply from kernel\n");
			close(nl_fd);
			return -1;
		}
		if (!NLMSG_OK((&nl_response_msg.n), nl_rxtx_length)) {
			printf("Error while receiving reply from kernel: Invalid Message\n");
			close(nl_fd);
			return -1;
		}
	
		//Parse the reply message
		nl_rxtx_length = GENLMSG_PAYLOAD(&nl_response_msg.n);
		nl_na = (struct nlattr *) GENLMSG_DATA(&nl_response_msg);
		printf("Kernel replied: %s\n",(char *)NLA_DATA(nl_na));
		
		buf = (char *)NLA_DATA(nl_na);
		
		if (strcmp(buf,"ERROR") && strcmp(buf,"QUEUE EMPTY")) {
			sscanf(buf, "%lu:%lu:%lu:%lu:v%d:t%d:s%lu",
				&ts.hour, &ts.min, &ts.sec, 
				&ts.nsec, &ts.valid, 
				&ts.tx_rx, &ts.seq);
			printf_myts(&ts);
		}
		sleep(1);
	}
	
	//Step 5. Close the socket and quit
	close(nl_fd);
	return 0;
}
