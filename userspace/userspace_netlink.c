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

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define NETLINK_USER 31

#define MAX_PAYLOAD 512 /* maximum payload size*/
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

#define NTIMES 100

struct myts {
		uint64_t sec;
		uint64_t nsec;
		uint64_t seq;
		int valid;
		int tx_rx;
};

#define MYNL_CMD_GETTS_TX 0
#define MYNL_CMD_GETTS_RX 1
#define MYNL_CMD_OK_RESP 2
#define MYNL_CMD_QEMPTY_RESP 3
#define MYNL_CMD_QERROR_RESP 4

struct mynl_cmd {
	int cmd;
	struct myts ts;
};

void printf_myts(struct myts *ts)
{
	printf("\n\n");
	printf("Got TS from Queue: \n");
	printf("sec: %lu \n",ts->sec);
	printf("nsec: %lu \n",ts->nsec);
	printf("seq: %lu \n",ts->seq);
	printf("valid: %d \n",ts->valid);
	printf("type: %s \n",(ts->tx_rx == 0) ? "Tx" : "Rx");
	printf("\n");
}

int main(int argc, char *argv[])
{
	int res;
	int i;
	int ntimes;
	char *buf;
	struct myts ts;
	struct mynl_cmd *cmd;
	
	if (argc < 2) {
		printf("Using %d times for the netlink test \n", NTIMES);
		ntimes = NTIMES;
	}
	else {
		ntimes = strtol(argv[1],(char **) NULL, 10);
	}
	
	sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
	if(sock_fd < 0) {
		printf("Error to open the socket.\n");
		return -1;
	}
	
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); /* self pid */

	bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0; /* For Linux Kernel */

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	cmd = (struct mynl_cmd *) NLMSG_DATA(nlh);

	/* Read message from kernel */
	for(i = 0 ; i < ntimes ; i++) {
		if (i % 2 == 0)
			cmd->cmd = MYNL_CMD_GETTS_TX;
		else
			cmd->cmd = MYNL_CMD_GETTS_RX;
			
		nlh->nlmsg_pid = getpid();
		sendmsg(sock_fd,&msg, 0); //MSG_DONTWAIT);
		recvmsg(sock_fd, &msg, 0); //MSG_DONTWAIT);
		
		cmd = (struct mynl_cmd *) NLMSG_DATA(nlh);
		
		if (cmd->cmd == MYNL_CMD_OK_RESP) {
			printf_myts(&(cmd->ts));
		} else if (cmd->cmd == MYNL_CMD_QEMPTY_RESP) {
			printf("\n\n");
			printf("Queue empty.\n");
			printf("\n");
		} else if (cmd->cmd == MYNL_CMD_QERROR_RESP) {
			printf("\n\n");
			printf("Queue error.\n");
			printf("\n");
		}
		//sleep(1);
	}
	
	free((void *) nlh);
	
	close(sock_fd);
}
