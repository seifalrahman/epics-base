/*
 * rational replacement for inet_addr()
 * 
 * author: Jeff Hill
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "osiSock.h"

#ifndef LOCAL
#define LOCAL static
#endif

#ifndef NELEMENTS
#define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif /*NELEMENTS*/

/*
 * addrArrayToUL ()
 */
LOCAL int addrArrayToUL (const unsigned short *pAddr, unsigned nElements, struct in_addr *pIpAddr)
{
	unsigned i;
	unsigned long addr = 0ul;
	
	for ( i=0u; i < nElements; i++ ) {
		if ( pAddr[i] > 0xff ) {
			return -1;
		}
		addr <<= 8;
		addr |= pAddr[i];
	}
	pIpAddr->s_addr = htonl ( addr );
		
	return 0;
}

/*
 * initIPAddr()
 * !! ipAddr should be passed in in network byte order !!
 * !! port is passed in in host byte order !!
 */
LOCAL int initIPAddr (struct in_addr ipAddr, unsigned short port, struct sockaddr_in *pIP)
{
	memset (pIP, '\0', sizeof(*pIP));
	pIP->sin_family = AF_INET;
	pIP->sin_port = htons(port);
	pIP->sin_addr = ipAddr;
	return 0;
}

/*
 * rational replacement for inet_addr()
 * which allows the limited broadcast address
 * 255.255.255.255, allows the user
 * to specify a port number, and allows also a
 * named host to be specified.
 *
 * Sets the port number to "defaultPort" only if 
 * "pAddrString" does not contain an address of the form
 * "n.n.n.n:p"
 */
epicsShareFunc int epicsShareAPI 
	aToIPAddr(const char *pAddrString, unsigned short defaultPort, struct sockaddr_in *pIP)
{
	int status;
	unsigned short addr[4];
    unsigned long rawAddr;
	char hostName[512]; /* !! change n elements here requires change in format below !! */
	unsigned short port;
	struct in_addr ina;

	/*
	 * traditional dotted ip addres
	 */
	status = sscanf (pAddrString, "%hu.%hu.%hu.%hu:%hu", 
			addr, addr+1u, addr+2u, addr+3u, &port);
	if (status>=4) {
		if ( addrArrayToUL ( addr, NELEMENTS ( addr ), &ina ) < 0 ) {
			return -1;
		}
		if (status==4) {
			port = defaultPort;
		}
		return initIPAddr (ina, port, pIP);
	}
	
	/*
	 * IP address as a raw number
	 */
	status = sscanf ( pAddrString, "%lu:%hu", &rawAddr, &port );
	if (status>=1) {
		if ( rawAddr > 0xffffffff ) {
			return -1;
		}
		if ( status == 1 ) {
			port = defaultPort;
		}
		ina.s_addr = htonl ( rawAddr );
		return initIPAddr ( ina, port, pIP );
	}

	/*
	 * check for a valid host name before giving up
	 */
	status = sscanf (pAddrString, "%511s:%hu", hostName, &port);
	if (status>=1) {
		if (status==1) {
			port = defaultPort;
		}
		status = hostToIPAddr (hostName, &ina);
		if (status==0) {
			return initIPAddr (ina, port, pIP);
		}
	}

	/*
	 * none of the above - return indicating failure
	 */
	return -1;
}
