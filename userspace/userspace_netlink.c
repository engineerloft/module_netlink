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

#define MAX_PAYLOAD 1024 /* maximum payload size*/
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

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
	printf("TX_RX: %d \n", ts->tx_rx);
	printf("Seq: %lu \n", ts->seq);
	printf("================================== \n");
}

int main(int argc, char *argv[])
{
	int res;
	int i;
	int ntimes;
	char *buf;
	struct myts ts;
	
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

	/* Read message from kernel */
	for(i = 0 ; i < ntimes ; i++) {
		printf("Copying buffer... \n");
		if (i % 2 == 0)
			strcpy(NLMSG_DATA(nlh), "GETTS_TX");
		else
			strcpy(NLMSG_DATA(nlh), "GETTS_RX");
			
		nlh->nlmsg_pid = getpid();
		printf("Sending buffer... \n");
		sendmsg(sock_fd,&msg, 0); //MSG_DONTWAIT);
		printf("Receiving buffer... \n");
		recvmsg(sock_fd, &msg, 0); //lMSG_DONTWAIT);
		buf = (char *)NLMSG_DATA(nlh);
		printf("Received message payload: %s\n", buf);
		
		if (strcmp(buf,"ERROR") && strcmp(buf,"QUEUE EMPTY")) {
			sscanf(buf, "%lu:%lu:%lu:%lu:v%d:t%d:s%lu",
				&ts.hour, &ts.min, &ts.sec, 
				&ts.nsec, &ts.valid, 
				&ts.tx_rx, &ts.seq);
			printf_myts(&ts);
		}
		sleep(1);
	}
	
	free((void *) nlh);
	
	close(sock_fd);
}
