/*
 * $Id$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>


#include "udp_server.h"
#include "config.h"
#include "dprint.h"


int udp_sock;



int udp_init(unsigned long ip, unsigned short port)
{
	struct sockaddr_in* addr;
	int optval;

	addr=(struct sockaddr_in*)malloc(sizeof(struct sockaddr));
	if (addr==0){
		DPrint("ERROR: udp_init: out of memory\n");
		goto error;
	}
	addr->sin_family=AF_INET;
	addr->sin_port=htons(port);
	addr->sin_addr.s_addr=ip;

	udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (udp_sock==-1){
		DPrint("ERROR: udp_init: socket: %s\n", strerror());
		goto error;
	}
	/* set sock opts? */
	optval=1;
	if (setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR,
					(void*)&optval, sizeof(optval)) ==-1)
	{
		DPrint("ERROR: udp_init: setsockopt: %s\n", strerror());
		goto error;
	}

	if (bind(udp_sock, (struct sockaddr*) addr, sizeof(struct sockaddr))==-1){
		DPrint("ERROR: udp_init: bind: %s\n", strerror());
		goto error;
	}

	free(addr);
	return 0;

error:
	if (addr) free(addr);
	return -1;
}



int udp_rcv_loop()
{
	int len;
	char buf[BUF_SIZE+1];
	struct sockaddr* from;
	int fromlen;

	from=(struct sockaddr*) malloc(sizeof(struct sockaddr));
	if (from==0){
		DPrint("ERROR: udp_rcv_loop: out of memory\n");
		goto error;
	}

	for(;;){
		fromlen=sizeof(struct sockaddr);
		len=recvfrom(udp_sock, buf, BUF_SIZE, 0, from, &fromlen);
		if (len==-1){
			DPrint("ERROR: udp_rcv_loop:recvfrom: %s\n", strerror());
			if (errno==EINTR)	goto skip;
			else goto error;
		}
		/*debugging, make print* msg work */
		buf[len+1]=0;

		receive_msg(buf, len, ((struct sockaddr_in*)from)->sin_addr.s_addr);
		
	skip: /* do other stuff */
		
	}
	
	if (from) free(from);
	return 0;
	
error:
	if (from) free(from);
	return -1;
}



/* which socket to use? main socket or new one? */
int udp_send(char *buf, int len, struct sockaddr*  to, int tolen)
{

	int n;
again:
	n=sendto(udp_sock, buf, len, 0, to, tolen);
	if (n==-1){
		DPrint("ERROR: udp_send: sendto: %s\n", strerror());
		if (errno==EINTR) goto again;
	}
	return n;
}
