#include<winsock2.h>
#include<WS2tcpip.h>

#include <mswsock.h>
#include<windows.h>
#include<iostream>
#include<stdlib.h>


#include "resolve.h"
#pragma comment(lib, "Ws2_32.lib")


#define DEFAULT_BUFFER_SIZE 4096
#define DEFAULT_OVERLAPPED_COUNT 5
#define MAX_OVERLAPPED_ACCEPTS 500
#define MAX_OVERLAPPED_SENDS 200
#define MAX_OVERLAPPED_RECVS 200
#define MAX_COMPLETION_THREAD_COUNT 32
#define BURST_ACCEPT_COUNT 100

using namespace std;

int gAddressFamily = AF_UNSPEC,
gSocketType = SOCK_STREAM,
gProtocol = IPPROTO_TCP,
gBufferSize = DEFAULT_BUFFER_SIZE,
gInitialAccepts = DEFAULT_OVERLAPPED_COUNT,
gMaxAccepts = MAX_OVERLAPPED_ACCEPTS,
gMaxReceives = MAX_OVERLAPPED_RECVS,
gMaxSends = MAX_OVERLAPPED_SENDS;

char* gBindAddr = NULL;
char* gBindPort;

volatile LONG gBytesRead = 0, gBytesSent = 0, gStartTime = 0, gBytesReadLast = 0, gBytesSentLast = 0,
gStartTimeLast = 0, gConnections = 0, gConnectionLast = 0, gOutstandingSends = 0;


typedef struct _BUFFER_OBJ
{
	WSAOVERLAPPED ol;
	SOCKET sclient;
	HANDLE PostAccept;
	char* buf;
	int buflen;
	int operation;
#define OP_ACCEPT 0
#define OP_READ 1
#define OP_WRITE 2
	
	SOCKADDR_STORAGE addr;
	int addrlen;
	struct _SOCKET_OBJ* sock;
	struct _BUFFER_OBJ* next;
} BUFFER_OBJ;

typedef struct _SOCKET_OBJ
{
	SOCKET s;
	int af;
	int bClosing;
	volatile LONG OutstandingRecv,
		OutstandingSend, PendingSend;
	CRITICAL_SECTION SockCritSec;
	struct _SOCKET_OBJ* next;
} SOCKET_OBJ;

typedef struct _LISTEN_OBJ
{
	SOCKET s;
	int AddressFamily;
	BUFFER_OBJ* PendingAccepts;
	volatile long PendingAcceptCount;
	int HiWateMark, LoWaterMark;
	HANDLE AcceptEvent;
	HANDLE RepostAccept;
	volatile long RepostCount;

	LPFN_ACCEPTEX lpfnAcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs;
	CRITICAL_SECTION ListenCritSec;
	struct _LISTEN_OBJ* next;
} LISTEN_OBJ;


CRITICAL_SECTION gBufferListCs, gSocketListCs, gPendingCritSec;

BUFFER_OBJ* gFreeBufferList = NULL;
SOCKET_OBJ* gFreeSocketList = NULL;
BUFFER_OBJ* gPendingSendList = NULL, * gPendingSendListEnd = NULL;

int PostSend(SOCKET_OBJ* sock, BUFFER_OBJ* sendobj);
int PostRecv(SOCKET_OBJ* sock, BUFFER_OBJ* recvobj);
void FreeBufferObj(BUFFER_OBJ* obj);
int usage(char* progname);
void dbgprint(char* format, ...);
void EnqueuePendingOperation(BUFFER_OBJ** head, BUFFER_OBJ** end, int op);
BUFFER_OBJ* DequeuePendingOperation(BUFFER_OBJ* listenobj, BUFFER_OBJ* obj);
void ProcessPendingOperation();
void InsertPendingAccept(LISTEN_OBJ* listenobj, BUFFER_OBJ* obj);
void RemovePendingAccept(LISTEN_OBJ* listenobj, BUFFER_OBJ* obj);

BUFFER_OBJ* GetBufferObj(int buflen);
SOCKET_OBJ* GetSocketObj(SOCKET s, int af);

void FreeSocketObj(SOCKET_OBJ* obj);
void ValidateArgs(int argc, char** argv);
void PrintStatistics();
int PostAccept(LISTEN_OBJ* listen, BUFFER_OBJ* acceptobj);
void HandleIo(ULONG_PTR key, BUFFER_OBJ* buf, HANDLE CompPort, DWORD BytesTransfered, DWORD error);
DWORD WINAPI CompletionThread(LPVOID lpParam);

int main(int argc, char* argv[]) {
	WSADATA wsd;
	SYSTEM_INFO sysinfo;
	LISTEN_OBJ* ListenSockets = NULL, * listenobj = NULL;
	SOCKET_OBJ* sockobj = NULL;
	BUFFER_OBJ* acceptobj = NULL;
	GUID guidAcceptEx = WSAID_ACCEPTEX, guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
	DWORD bytes;
	HANDLE CompletionPort, WaitEvents[MAX_COMPLETION_THREAD_COUNT], hrc;
	int endpointcount = 0, waitcount = 0, interval, rc, i;
	struct addrinfo* res = NULL, * ptr = NULL;

	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}

	ValidateArgs(argc, argv);

	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0) {
		fprintf(stderr, "unable to load Winsock!\n");
		return -1;
	}

	InitializeCriticalSection(&gSocketListCs);
	InitializeCriticalSection(&gBufferListCs);
	InitializeCriticalSection(&gPendingCritSec);

	CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)NULL, 0);
	if (CompletionPort == NULL)
	{
		fprintf(stderr, "CreateCompletionPort failed: %d\n", GetLastError());
		return -1;
	}

	GetSystemInfo(&sysinfo);
	if (sysinfo.dwNumberOfProcessors > MAX_COMPLETION_THREAD_COUNT) {
		sysinfo.dwNumberOfProcessors = MAX_COMPLETION_THREAD_COUNT;
	}

	printf("Buffer size= %lu (page size = %lu)\n", gBufferSize, sysinfo.dwPageSize);

	for (waitcount = 0; waitcount < (int)sysinfo.dwNumberOfProcessors; waitcount++) {
		WaitEvents[waitcount] = CreateThread(NULL, 0, CompletionThread, (LPVOID)CompletionPort, 0, NULL);
		if (WaitEvents[waitcount] == NULL)
		{
			fprintf(stderr, "CreateThread failed: %d\n", GetLastError());
			return -1;
		}
	}
	
	printf("Local address: %s; Port: %s; Family: %d\n", gBindAddr, gBindPort, gAddressFamily);

	res = ResolveAddress(gBindAddr, gBindPort, gAddressFamily, gSocketType, gProtocol);
	if (res == NULL) {
		fprintf(stderr, "ResolveAddress failed to return any address!\n");
		return -1;
	}

	ptr = res;
	while (ptr)
	{
		printf("Listening address: ");
		PrintAddress(ptr->ai_addr, ptr->ai_addrlen);
		printf("\n");
		listenobj = (LISTEN_OBJ*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(LISTEN_OBJ));
		if (listenobj == NULL)
		{
			fprintf(stderr, "out of memory!\n");
			return -1;
		}

		listenobj->LoWaterMark = gInitialAccepts;

		listenobj->s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (listenobj->s == INVALID_SOCKET)
		{
			fprintf(stderr, "socketfailed: %d\n", WSAGetLastError());
			return -1;
		}
		listenobj->AcceptEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (listenobj->AcceptEvent == NULL) {
			fprintf(stderr, "CreateEvent failed: %d\n", WSAGetLastError());
			return -1;
		}

		listenobj->RepostAccept = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (listenobj->RepostAccept == NULL)
		{
			fprintf(stderr, "CreateSemaphore failed: %d\n", GetLastError());
			return -1;
		}

		WaitEvents[waitcount++] = listenobj->AcceptEvent;
		WaitEvents[waitcount++] = listenobj->RepostAccept;

		hrc = CreateIoCompletionPort((HANDLE)listenobj->s, CompletionPort, (ULONG_PTR)listenobj, 0);

		if (hrc == NULL)
		{
			fprintf(stderr, "CreateIoCompletionPort failed: %d\n", GetLastError());
			return -1;
		}

		rc = bind(listenobj->s, ptr->ai_addr, ptr->ai_addrlen);
		if (rc == SOCKET_ERROR)
		{
			fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
			return -1;
		}

		rc = WSAIoctl(listenobj->s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx), &listenobj->lpfnAcceptEx, sizeof(listenobj->lpfnAcceptEx), &bytes, NULL, NULL);

		if (rc == SOCKET_ERROR)
		{
			fprintf(stderr, "WSAIoctl: SIO_GET_EXTENSION_FUNCTION_POINTER failed: %d\n", WSAGetLastError());
			return -1;
		}

		rc = WSAIoctl(listenobj->s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs), &listenobj->lpfnGetAcceptExSockaddrs, sizeof(listenobj->lpfnGetAcceptExSockaddrs), &bytes, NULL, NULL);
		if (rc == SOCKET_ERROR) {
			fprintf(stderr, "WSAIoctl: SIO_GET_EXTENSION_FUNCTION_POINTER failed: %d\n", WSAGetLastError());
			return -1;
		}

		rc = listen(listenobj->s, 200);
		if (rc == SOCKET_ERROR)
		{
			fprintf(stderr, "listen failed: %d\n", WSAGetLastError());
			return -1;
		}

		rc = WSAEventSelect(listenobj->s, listenobj->AcceptEvent, FD_ACCEPT);
		if (rc == SOCKET_ERROR) {
			fprintf(stderr, "WSAEventSelect failed: %d\n", WSAGetLastError());
			return -1;
		}

		for (i = 0; i < gInitialAccepts; ++i)
		{
			acceptobj = GetBufferObj(gBufferSize);
			if (acceptobj == NULL) {
				fprintf(stderr, "Out of memory!\n");
				return -1;
			}

			acceptobj->PostAccept = listenobj->AcceptEvent;
			InsertPendingAccept(listenobj, acceptobj);
			PostAccept(listenobj, acceptobj);
		}
		
		if (ListenSockets == NULL)
		{
			ListenSockets = listenobj;
		}
		else {
			listenobj->next = ListenSockets;
			ListenSockets = listenobj;
		}

		endpointcount++;
		ptr = ptr->ai_next;
	}

	freeaddrinfo(res);
	gStartTime = gStartTimeLast = GetTickCount();
	interval = 0;
	while (1)
	{
		rc = WSAWaitForMultipleEvents(waitcount, WaitEvents, FALSE, 5000, FALSE);
		if (rc == WAIT_FAILED)
		{
			fprintf(stderr, "WSAGetForMultipleEvents failed: %d\n", WSAGetLastError());
			break;
		}
		else if (rc == WAIT_TIMEOUT)
		{
			interval++;
			PrintStatistics();
			if (interval == 36) {
				int optval, optlen;

				listenobj = ListenSockets;
				while (listenobj)
				{
					EnterCriticalSection(&listenobj->ListenCritSec);
					acceptobj = listenobj->PendingAccepts;

					while (acceptobj)
					{
						optlen = sizeof(optval);
						rc = getsockopt(acceptobj->sclient, SOL_SOCKET, SO_CONNECT_TIME, (char*)&optval, &optlen);
						if (rc == SOCKET_ERROR)
						{
							fprintf(stderr, "getsockopt: SO_CONNECT_TIME failed: %d\n", WSAGetLastError());
						}
						else {
							if ((optval != 0xFFFFFFFF) && (optval > 300))
							{
								printf("closing stale handle\n");
								closesocket(acceptobj->sclient);
								acceptobj->sclient = INVALID_SOCKET;
							}
						}
						acceptobj = acceptobj->next;
					}
					LeaveCriticalSection(&listenobj->ListenCritSec);
					listenobj = listenobj->next;
				}
				interval = 0;
			}
		}
		else
		{
			int index;
			index = rc - WAIT_OBJECT_0;
			for (; index < waitcount; index++)
			{
				rc = WaitForSingleObject(WaitEvents[index], 0);
				if (rc == WAIT_FAILED || rc == WAIT_TIMEOUT)
				{
					continue;
				}
				if (index < (int)sysinfo.dwNumberOfProcessors)
				{
					ExitProcess(-1);
				}
				else
				{
					listenobj = ListenSockets;
					while (listenobj)
					{
						if ((listenobj->AcceptEvent == WaitEvents[index]) || (listenobj->RepostAccept == WaitEvents[index]))
							break;

						listenobj = listenobj->next;
					}

					if (listenobj)
					{
						WSANETWORKEVENTS ne;
						int limit = 0;
						if (listenobj->AcceptEvent == WaitEvents[index])
						{
							rc = WSAEnumNetworkEvents(listenobj->s, listenobj->AcceptEvent, &ne);
							if (rc == SOCKET_ERROR)
							{
								fprintf(stderr, "WSAEnumNetworkEvents failed: %d\n", WSAGetLastError());
							}
							if ((ne.lNetworkEvents & FD_ACCEPT) == FD_ACCEPT)
							{
								limit = BURST_ACCEPT_COUNT;
							}
						}
						else if (listenobj->RepostAccept == WaitEvents[index])
						{
							limit = InterlockedExchange(&listenobj->RepostCount, 0);
							ResetEvent(listenobj->RepostAccept);
						}
						i = 0;
						while ((i++ < limit) && (listenobj->PendingAcceptCount < gMaxAccepts))
						{
							acceptobj = GetBufferObj(gBufferSize);
							if (acceptobj) {
								acceptobj->PostAccept = listenobj->AcceptEvent;
								InsertPendingAccept(listenobj, acceptobj);
								PostAccept(listenobj, acceptobj);
							}
						}
					}
				}
			}
		}
	}

	WSACleanup();
	return 0;
}

int usage(char* progname) {
	fprintf(stderr, "Usage: %s [-a 4|6] [-e port] [-l local-addr] [-p udp|tcp]\n", progname);
	fprintf(stderr, "  -a  4|6     Address family, 4 = IPv4, 6 = IPv6 [default = IPv4]\n"
		"else will listen to both IPv4 and IPv6\n"
		"  -b  size    Buffer size for send/recv [default = %d]\n"
		"  -e  port    Port number [default = %s]\n"
		"  -l  addr    Local address to bind to [default INADDR_ANY for IPv4 or INADDR6_ANY for IPv6]\n"
		"  -oa count   Maximum overlapped accepts to allow\n"
		"  -os count   Maximum overlapped sends to allow\n"
		"  -or count   Maximum overlapped receives to allow\n"
		"  -o  count   Initial number of overlapped accepts to post\n",
		gBufferSize,
		gBindPort
	);
	return 0;
}

void dbgprint(char* format, ...) {
#ifdef DEBUG
	va_list vl;
	char dbgbuf[2048];
	va_start(vl, format);
	wvsprintf(dbgbuf, format, vl);
	va_end(vl);
	printf(dbgbuf);
	OutputDebugString(dbgbuf);
#endif
}

void EnqueuePendingOperation(BUFFER_OBJ** head, BUFFER_OBJ** end, BUFFER_OBJ* obj, int op)
{
	EnterCriticalSection(&gPendingCritSec);

	if (op == OP_READ);
	else if(op == OP_WRITE)
		InterlockedIncrement(&obj->sock->PendingSend);
	obj->next = NULL;
	if (*end)
	{
		(*end)->next = obj;
		(*end) = obj;
	}
	else {
		(*head) = (*end) = obj;
	}
	LeaveCriticalSection(&gPendingCritSec);

	return;
}

BUFFER_OBJ* DequeuePendingOperation(BUFFER_OBJ** head, BUFFER_OBJ** end, int op)
{
	BUFFER_OBJ* obj = NULL;
	EnterCriticalSection(&gPendingCritSec);

	if (*head)
	{
		obj = *head;
		(*head) = obj->next;

		if (obj->next == NULL) {
			(*end) = NULL;
		}
		if (op == OP_READ);
		if (op == OP_WRITE)
			InterlockedDecrement(&obj->sock->PendingSend);
	}
	LeaveCriticalSection(&gPendingCritSec);
	return obj;
}

void ProcessPendingOperations()
{
	BUFFER_OBJ* sendobj = NULL;
	while (gOutstandingSends < gMaxSends) {
		sendobj = DequeuePendingOperation(&gPendingSendList, &gPendingSendListEnd, OP_WRITE);
		if (sendobj)
		{
			if (PostSend(sendobj->sock, sendobj) == SOCKET_ERROR)
			{
				printf("ProcessPendingOperations: PostSend failed!\n");
				FreeBufferObj(sendobj);
				break;
			}
		}
		else {
			break;
		}
	}
	return;
}

void InsertPendingAccept(LISTEN_OBJ* listenobj, BUFFER_OBJ* obj)
{
	obj->next = NULL;
	EnterCriticalSection(&listenobj->ListenCritSec);
	if (listenobj->PendingAccepts == NULL) {
		listenobj->PendingAccepts = obj;
	}
	else {
		obj->next = listenobj->PendingAccepts;
		listenobj->PendingAccepts = obj;
	}
	LeaveCriticalSection(&listenobj->ListenCritSec);
}

void RemovePendingAccept(LISTEN_OBJ* listenobj, BUFFER_OBJ* obj)
{
	BUFFER_OBJ* ptr = NULL, * prev = NULL;
	EnterCriticalSection(&listenobj->ListenCritSec);

	ptr = listenobj->PendingAccepts;
	while ((ptr) && (ptr != obj))
	{
		prev = ptr;
		ptr = ptr->next;
	}
	if (prev)
	{
		prev->next = obj->next;
	}
	else
	{
		listenobj->PendingAccepts = obj->next;
	}
	LeaveCriticalSection(&listenobj->ListenCritSec);
}

BUFFER_OBJ* GetBufferObj(int buflen)
{
	BUFFER_OBJ* newobj = NULL;
	EnterCriticalSection(&gBufferListCs);
	if (gFreeBufferList == NULL)
	{
		newobj = (BUFFER_OBJ*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BUFFER_OBJ) + (sizeof(BYTE) * buflen));
		if (newobj == NULL)
		{
			fprintf(stderr, "GetBufferObj: HeapAlloc failed: %d\n", GetLastError());
		}
	}
	else {
		newobj = gFreeBufferList;
		gFreeBufferList = newobj->next;
		newobj->next = NULL;
	}
	LeaveCriticalSection(&gBufferListCs);

	if (newobj)
	{
		newobj->buf = (char*)(((char*)newobj) + sizeof(BUFFER_OBJ));
		newobj->buflen = buflen;
		newobj->addrlen = sizeof(newobj->addr);
	}

	return newobj;
}

void FreeBufferObj(BUFFER_OBJ* obj) {
	EnterCriticalSection(&gBufferListCs);
	memset(obj, 0, sizeof(BUFFER_OBJ) + gBufferSize);
	obj->next = gFreeBufferList;
	gFreeBufferList = obj;
	LeaveCriticalSection(&gBufferListCs);
}

SOCKET_OBJ* GetSocketObj(SOCKET s, int af)
{
	SOCKET_OBJ* sockobj = NULL;
	EnterCriticalSection(&gSocketListCs);
	if (gFreeSocketList == NULL)
	{
		sockobj = (SOCKET_OBJ*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SOCKET_OBJ));
		if (sockobj == NULL)
		{
			fprintf(stderr, "GetSocketObj: HeapAlloc failed: %d\n", GetLastError());

		}
		else {
			InitializeCriticalSection(&sockobj->SockCritSec);
		}
	}
	else {
		sockobj = gFreeSocketList;
		gFreeSocketList = sockobj->next;
		sockobj->next = NULL;
	}
	LeaveCriticalSection(&gSocketListCs);

	if (sockobj) {
		sockobj->s = s;
		sockobj->af = af;
	}
	return sockobj;
}

void FreeSocketObj(SOCKET_OBJ* obj) {
	CRITICAL_SECTION cstmp;
	BUFFER_OBJ* ptr = NULL;

	if (obj->s != INVALID_SOCKET) {
		printf("FreeSocketObj: closing socket\n");
		closesocket(obj->s);
		obj->s = INVALID_SOCKET;
	}

	EnterCriticalSection(&gSocketListCs);
	cstmp = obj->SockCritSec;
	memset(obj, 0, sizeof(SOCKET_OBJ));
	obj->SockCritSec = cstmp;
	obj->next = gFreeSocketList;
	gFreeSocketList = obj;
	LeaveCriticalSection(&gSocketListCs);
}

void ValidateArgs(int argc, char** argv)
{
	int i;

	for (i = 1; i < argc; i++)
	{
		if (((argv[i][0] != '/') && (argv[i][0] != '-')) || (strlen(argv[i]) < 2))
			usage(argv[0]);
		else
		{
			switch (tolower(argv[i][1]))
			{
			case 'a':
				if (i + 1 >= argc)
					usage(argv[0]);
				if (argv[i + 1][0] == '4')
					gAddressFamily = AF_INET6;
				else
					usage(argv[0]);
				i++;
				break;

			case 'b':
				if (i + 1 >= argc)
					usage(argv[0]);
				gBufferSize = atol(argv[++i]);
				break;

			case 'e':
				if (i + 1 >= argc)
					usage(argv[0]);
				gBindPort = argv[++i];
				break;
			case 'l':
				if (i + 1 >= argc)
					usage(argv[0]);
				gBindAddr = argv[++i];
				break;

			case 'o':
				if (i + 1 >= argc)
					usage(argv[0]);
				if (strlen(argv[i]) == 2)
				{
					gInitialAccepts = atol(argv[++i]);
				}
				else if (strlen(argv[i]) == 3)
				{
					if (tolower(argv[i][2]) == 'a')
						gMaxAccepts = atol(argv[++i]);
					else if (tolower(argv[i][2]) == 's')
						gMaxSends = atol(argv[++i]);
					else if (tolower(argv[i][2]) == 'r')
						gMaxReceives = atol(argv[++i]);
					else
						usage(argv[0]);
				}
				else 
				{
					usage(argv[0]);
				}
				break;

			default:
				usage(argv[0]);
				break;
			}
		}
	}
}

void PrintStatistics()
{
	ULONG bps, tick, elapsed, cps;
	tick = GetTickCount();
	elapsed = (tick - gStartTime) / 1000;

	if (elapsed == 0)
		return;
	printf("\n");

	bps = gBytesSent / elapsed;
	printf("Average BPS sent: %lu [%lu]\n", bps, gBytesSent);
	bps = gBytesRead / elapsed;
	printf("Average BPS read: %lu [%lu]\n", bps, gBytesRead);
	elapsed = (tick - gStartTimeLast) / 1000;

	if (elapsed == 0)
		return;

	bps = gBytesSentLast / elapsed;
	printf("Current BPS sent: %lu\n", bps);
	bps = gBytesReadLast / elapsed;
	printf("Current BPS read: %lu\n", bps);
	cps = gConnectionLast / elapsed;
	printf("Current conns/sec: %lu\n", cps);
	printf("Total connections: %lu\n", gConnections);
	InterlockedExchange(&gBytesSentLast, 0);
	InterlockedExchange(&gBytesReadLast, 0);
	InterlockedExchange(&gConnectionLast, 0);
	gStartTimeLast = tick;
}

int PostRecv(SOCKET_OBJ* sock, BUFFER_OBJ* recvobj) {
	WSABUF wbuf;
	DWORD bytes, flags;
	int rc;

	recvobj->operation = OP_READ;
	wbuf.buf = recvobj->buf;
	wbuf.len = recvobj->buflen;
	flags = 0;
	EnterCriticalSection(&sock->SockCritSec);
	rc = WSARecv(sock->s, &wbuf, 1, &bytes, &flags, &recvobj->ol, NULL);

	if (rc == SOCKET_ERROR)
	{
		rc = NO_ERROR;
		if (WSAGetLastError() != WSA_IO_PENDING) {
			dbgprint((char*)"PostRecv: WSARecv* failed: %d\n", WSAGetLastError());
			rc = SOCKET_ERROR;
		}
	}
	if (rc == NO_ERROR)
	{
		InterlockedIncrement(&sock->OutstandingRecv);
	}
	LeaveCriticalSection(&sock->SockCritSec);
	return rc;
}

int PostSend(SOCKET_OBJ* sock, BUFFER_OBJ* sendobj)
{
	WSABUF wbuf;
	DWORD bytes;
	int rc, err;

	sendobj->operation = OP_WRITE;
	wbuf.buf = sendobj->buf;
	wbuf.len = sendobj->buflen;
	EnterCriticalSection(&sock->SockCritSec);
	rc = WSASend(sock->s, &wbuf, 1, &bytes, 0, &sendobj->ol, NULL);

	if (rc == SOCKET_ERROR)
	{
		rc = NO_ERROR;
		if ((err = WSAGetLastError()) != WSA_IO_PENDING)
		{
			if (err == WSAENOBUFS) {
				DebugBreak();
			}
			dbgprint((char*)"PostSend: WSASend* failed: %d [internal = %d]\n", WSAGetLastError(), sendobj->ol.Internal);
			rc = SOCKET_ERROR;
		}
	}

	if (rc == NO_ERROR) {
		InterlockedIncrement(&sock->OutstandingSend);
		InterlockedIncrement(&gOutstandingSends);
	}
	LeaveCriticalSection(&sock->SockCritSec);
	return rc;
}

int PostAccept(LISTEN_OBJ* listen, BUFFER_OBJ* acceptobj)
{
	DWORD bytes;
	int rc;

	acceptobj->operation = OP_ACCEPT;

	acceptobj->sclient = socket(listen->AddressFamily, SOCK_STREAM, IPPROTO_TCP);
	if (acceptobj->sclient == INVALID_SOCKET)
	{
		fprintf(stderr, "PosAccept: socket failed: %d\n", WSAGetLastError());
		return -1;
	}
	rc = listen->lpfnAcceptEx(
		listen->s,
		acceptobj->sclient,
		acceptobj->buf,
		acceptobj->buflen -((sizeof(SOCKADDR_STORAGE) + 16)*2),
		sizeof(SOCKADDR_STORAGE) +16,
		sizeof(SOCKADDR_STORAGE) +16,
		&bytes,
		&acceptobj->ol
	     );
	if (rc == FALSE)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("PostAccept: AcceptEx failed: %d\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
	}

	InterlockedIncrement(&listen->PendingAcceptCount);
	return NO_ERROR;
}

void HandleIo(ULONG_PTR key, BUFFER_OBJ* buf, HANDLE CompPort, DWORD BytesTransfered, DWORD error)
{
	LISTEN_OBJ* listenobj = NULL;
	SOCKET_OBJ* sockobj = NULL;
	SOCKET_OBJ* clientobj = NULL;

	BUFFER_OBJ* recvobj = NULL;
	BUFFER_OBJ* sendobj = NULL;
	BOOL bCleanupSocket;

	if (error != 0)
	{
		dbgprint((char*)"OP = %d; Error = %d\n", buf->operation, error);
	}
	bCleanupSocket = FALSE;

	if (error != NO_ERROR)
	{
		if (buf->operation != OP_ACCEPT)
		{
			sockobj = (SOCKET_OBJ*)key;
			if (buf->operation == OP_READ)
			{
				if ((InterlockedDecrement(&sockobj->OutstandingRecv) == 0) && (sockobj->OutstandingSend == 0)) {
					dbgprint((char*)"Freeing socket obj in GetOverlappedResult\n");
					FreeSocketObj(sockobj);
				}
			}
			else if(buf->operation == OP_WRITE)
			{
				if ((InterlockedDecrement(&sockobj->OutstandingSend) == 0) && (sockobj->OutstandingRecv == 0))
				{
					dbgprint((char*)"Freeing socket obj in GetOverlappedResult\n");
					FreeSocketObj(sockobj);
				}
			}
		}
		else
		{
			listenobj = (LISTEN_OBJ*)key;
			printf("Accept failed\n");
			buf->sclient = INVALID_SOCKET;
		}
		FreeBufferObj(buf);
		return;
	}

	if (buf->operation == OP_ACCEPT)
	{
		HANDLE hrc;
		SOCKADDR_STORAGE* LocalSockaddr = NULL, * RemoteSockaddr = NULL;
		int LocalSockaddrLen, RemoteSockaddrLen;
		listenobj = (LISTEN_OBJ*)key;

		InterlockedIncrement(&gConnections);
		InterlockedIncrement(&gConnectionLast);
		InterlockedDecrement(&listenobj->PendingAcceptCount);
		InterlockedExchangeAdd(&gBytesRead, BytesTransfered);
		InterlockedExchangeAdd(&gBytesReadLast, BytesTransfered);

		listenobj->lpfnGetAcceptExSockaddrs(
			buf->buf,
			buf->buflen - ((sizeof(SOCKADDR_STORAGE) + 16) * 2),
			sizeof(SOCKADDR_STORAGE) + 16,
			sizeof(SOCKADDR_STORAGE) + 16,
			(SOCKADDR**)&LocalSockaddr,
			&LocalSockaddrLen,
			(SOCKADDR**)&RemoteSockaddr,
			&RemoteSockaddrLen
		);
		RemovePendingAccept(listenobj, buf);

		clientobj = GetSocketObj(buf->sclient, listenobj->AddressFamily);
		if (clientobj)
		{
			hrc = CreateIoCompletionPort((HANDLE)clientobj->s, CompPort, (ULONG_PTR)clientobj, 0);
			if (hrc == NULL)
			{
				fprintf(stderr, "CompletionThread: CreateIoCompletionPort failed: %d\n", GetLastError());
				return;
			}

			sendobj = buf;
			sendobj->buflen = BytesTransfered;
			sendobj->sock = clientobj;

			EnqueuePendingOperation(&gPendingSendList, &gPendingSendListEnd, sendobj, OP_WRITE);
		}
		else
		{
			closesocket(buf->sclient);
			buf->sclient = INVALID_SOCKET;
			FreeBufferObj(buf);
		}

		if (error != NO_ERROR)
		{
			EnterCriticalSection(&clientobj->SockCritSec);
			if ((clientobj->OutstandingSend == 0) && (clientobj->OutstandingRecv == 0))
			{
				closesocket(clientobj->s);
				clientobj->s = INVALID_SOCKET;
				FreeSocketObj(clientobj);
			}
			else {
				clientobj->bClosing = TRUE;
			}
			LeaveCriticalSection(&clientobj->SockCritSec);
			error = NO_ERROR;
		}
		InterlockedIncrement(&listenobj->RepostCount);
		SetEvent(listenobj->RepostAccept);
	}
	else if(buf->operation == OP_READ)
	{
		sockobj = (SOCKET_OBJ*)key;
		InterlockedDecrement(&sockobj->OutstandingRecv);

		if (BytesTransfered > 0)
		{
			InterlockedExchangeAdd(&gBytesRead, BytesTransfered);
			InterlockedExchangeAdd(&gBytesReadLast, BytesTransfered);

			sendobj = buf;
			sendobj->buflen = BytesTransfered;
			sendobj->sock = sockobj;

			EnqueuePendingOperation(&gPendingSendList, &gPendingSendListEnd, sendobj, OP_WRITE);

		}
		else
		{
			sockobj->bClosing = TRUE;

			FreeBufferObj(buf);

			EnterCriticalSection(&sockobj->SockCritSec);
			if ((sockobj->OutstandingSend == 0) && (sockobj->OutstandingRecv == 0))
			{
				bCleanupSocket = TRUE;
			}
			LeaveCriticalSection(&sockobj->SockCritSec);
		}
	}
	else if(buf->operation == OP_WRITE)
	{
		sockobj = (SOCKET_OBJ*)key;
		InterlockedDecrement(&sockobj->OutstandingSend);
		InterlockedDecrement(&gOutstandingSends);
		// Update the counters
		InterlockedExchangeAdd(&gBytesSent, BytesTransfered);
		InterlockedExchangeAdd(&gBytesSentLast, BytesTransfered);

		buf->buflen = gBufferSize;
		if (sockobj->bClosing == FALSE)
		{
			buf->sock = sockobj;
			PostRecv(sockobj, buf);
		}
	}

	ProcessPendingOperations();
	if (sockobj)
	{
		if (error != NO_ERROR)
		{
			printf("err = %d\n", error);
			sockobj->bClosing = TRUE;
		}
		
		if ((sockobj->OutstandingSend == 0) && (sockobj->OutstandingRecv == 0) && (sockobj->bClosing))
		{
			bCleanupSocket = TRUE;
		}

		if (bCleanupSocket)
		{
			closesocket(sockobj->s);
			sockobj->s = INVALID_SOCKET;
			FreeSocketObj(sockobj);
		}
	}

	return;
}

DWORD WINAPI CompletionThread(LPVOID lpParam) {
	ULONG_PTR Key;
	SOCKET s;
	BUFFER_OBJ* bufobj = NULL;
	OVERLAPPED* lpOverlapped = NULL;
	HANDLE CompletionPort;
	DWORD BytesTransfered;
	DWORD Flags;
	int rc, error;

	CompletionPort = (HANDLE)lpParam;
	while (1)
	{
		error = NO_ERROR;
		rc = GetQueuedCompletionStatus(CompletionPort, &BytesTransfered, (PULONG_PTR)&Key, &lpOverlapped, INFINITE);
		bufobj = CONTAINING_RECORD(lpOverlapped, BUFFER_OBJ, ol);

		if (rc == FALSE)
		{
			if (bufobj->operation == OP_ACCEPT)
			{
				s = ((LISTEN_OBJ*)Key)->s;
			}
			else {
				s = ((SOCKET_OBJ*)Key)->s;
			}
			dbgprint((char*)"CompletionThread: GetQueuedCompletionStatus failed: %d [0x%x]\n", GetLastError(), lpOverlapped->Internal);
			rc = WSAGetOverlappedResult(s, &bufobj->ol, &BytesTransfered, FALSE, &Flags);
			if (rc == FALSE)
			{
				error = WSAGetLastError();
			}
		}
		HandleIo(Key, bufobj, CompletionPort, BytesTransfered, error);
	}

	ExitThread(0);
	return 0;
}
