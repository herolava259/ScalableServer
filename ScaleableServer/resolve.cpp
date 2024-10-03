#include<Winsock2.h>
#include<WS2tcpip.h>
#include<stdio.h>
#include<stdlib.h>
#include"resolve.h"
#pragma comment(lib, "Ws2_32.lib")

int PrintAddress(SOCKADDR* sa, int salen) {
	char host[NI_MAXHOST], serv[NI_MAXSERV];
	int hostlen = NI_MAXHOST, servlen = NI_MAXSERV, rc;
	rc = getnameinfo(sa, salen, host, hostlen, serv, servlen, NI_NUMERICHOST | NI_NUMERICSERV);

	if (rc != 0)
	{
		fprintf(stderr, "%s: getnameinfo() failed with error code %d\n", __FILE__, rc);
		return rc;
	}
	else
	{
		printf("getnameinfo() is OK!\n");
	}

	if (strcmp(serv, "0") != 0)
	{
		if (sa->sa_family == AF_INET)
			printf("[%s]:%s", host, serv);
		else
			printf("%s:%s", host, serv);
	}
	else
		printf("%s", host);

	return NO_ERROR;
}

int FormatAddress(SOCKADDR* sa, int salen, char* addrbuf, int addrbuflen)
{
	char    host[NI_MAXHOST], serv[NI_MAXSERV];
	int     hostlen = NI_MAXHOST, servlen = NI_MAXSERV, rc;

	rc = getnameinfo(sa, salen, host, hostlen, serv, servlen, NI_NUMERICHOST | NI_NUMERICSERV);
	if (rc != 0)
	{
		fprintf(stderr, "%s: getnameinfo() failed with error code %d\n", __FILE__, rc);
		return rc;
	}
	else
		printf("getnameinfo() is OK!\n");

	if ((strlen(host) + strlen(serv) + 1) > (unsigned)addrbuflen)
		return WSAEFAULT;
	if (sa->sa_family == AF_INET)
		sprintf_s(addrbuf, sizeof(addrbuf), "%s:%s", host, serv);
	else if (sa->sa_family == AF_INET6)
		sprintf_s(addrbuf, sizeof(addrbuf), "[%s]:%s", host, serv);
	else
		addrbuf[0] = '\0';

	return NO_ERROR;
}

struct addrinfo* ResolveAddress(char* addr, char* port, int af, int type, int proto)
{
	struct addrinfo hints, * res = NULL;
	int    rc;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = ((addr) ? 0 : AI_PASSIVE);
	hints.ai_family = af;
	hints.ai_socktype = type;
	hints.ai_protocol = proto;

	rc = getaddrinfo(addr, port, &hints, &res);
	if (rc != 0)
	{
		printf("Invalid address %s, getaddrinfo() failed with error code %d\n", addr, rc);
		return NULL;
	}
	else
		printf("getaddrinfo() should be fine!\n");

	return res;
}