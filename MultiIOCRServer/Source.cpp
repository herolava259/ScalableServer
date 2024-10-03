#include<WinSock2.h>
#include<windows.h>
#include<iostream>
#include<process.h>
#include<WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define RECEIVE 0
#define SEND 1
#define PORT 5500
#define DATA_BUFSIZE 8192
#define MAX_CLIENT 1024
#define SERVER_ADDR "127.0.0.1"

using namespace std;

typedef char* _TCHAR;

typedef struct{
	WSAOVERLAPPED overlapped;
	SOCKET socket;
	WSABUF dataBuf;
	char buffer[DATA_BUFSIZE];
	int operation;
	int sentBytes;
	int recvBytes;
}SocketInfo;

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED overlapped, DWORD inFlags);
unsigned __stdcall ioThread(LPVOID lpParameter);

SOCKET acceptSocket;
SocketInfo* clients[MAX_CLIENT];
int nClients = 0;
CRITICAL_SECTION criticalSection;

int main(int argc, _TCHAR* argv[]) {
	WSADATA wsaData;
	SOCKET listenSocket;
	SOCKADDR_IN serverAddr, clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	INT ret;
	WSAEVENT acceptEvent;

	InitializeCriticalSection(&criticalSection);
	if ((ret = WSAStartup((2, 2), &wsaData) != 0)){
		cout << "WSAStartup() failed with error " << ret << endl;
		WSACleanup();
		return 1;
	}
	if ((listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		cout << "Failed to get a socket " << WSAGetLastError() << endl;
		return 1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
	if (bind(listenSocket, (PSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "bind() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	if (listen(listenSocket, 20)) {
		cout << "listen() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	cout << "Server started!" << endl;
	if ((acceptEvent = WSACreateEvent()) == WSA_INVALID_EVENT) {
		cout << "WSACreateEvent() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	_beginthreadex(0, 0, ioThread, (LPVOID)acceptEvent, 0, 0);

	while (1) {
		if ((acceptSocket = accept(listenSocket, (PSOCKADDR)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
			cout << "accept() failed with error " << WSAGetLastError() << endl;
			return 1;
		}

		if (WSASetEvent(acceptEvent) == FALSE)
		{
			cout << "WSASetEvent() failed with error " << WSAGetLastError() << endl;
			return 1;
		}
	}

	DeleteCriticalSection(&criticalSection);
	closesocket(listenSocket);
	WSACleanup();
	return 0;
}

unsigned __stdcall ioThread(LPVOID lpParameter) {
	DWORD flags;
	WSAEVENT events[1];
	DWORD index;
	DWORD recvBytes;

	events[0] = (WSAEVENT)lpParameter;

	while (TRUE) {
		while (TRUE) {
			index = WSAWaitForMultipleEvents(1, events, FALSE, WSA_INFINITE, TRUE);

			if (index == WSA_WAIT_FAILED) {
				cout << "WSAWaitForMultipleEvents() failed with error " << WSAGetLastError() << endl;
				return 1;
			}

			if (index != WAIT_IO_COMPLETION) {
				break;
			}
		}

		WSAResetEvent(events[index - WSA_WAIT_EVENT_0]);

		EnterCriticalSection(&criticalSection);

		if (nClients == MAX_CLIENT) {
			cout << "TOO many clients" << endl;
			closesocket(acceptSocket);
			continue;
		}

		if ((clients[nClients] = (SocketInfo*)GlobalAlloc(GPTR, sizeof(SocketInfo))) == NULL) {
			cout << "GlobalAlloc() failed with error " << WSAGetLastError() << endl;
			return 1;
		}

		clients[nClients]->socket = acceptSocket;
		memset(&clients[nClients]->overlapped, 0, sizeof(WSAOVERLAPPED));
		clients[nClients]->sentBytes = 0;
		clients[nClients]->recvBytes = 0;
		clients[nClients]->dataBuf.len = DATA_BUFSIZE;
		clients[nClients]->dataBuf.buf = clients[nClients]->buffer;
		clients[nClients]->operation = RECEIVE;
		flags = 0;

		if (WSARecv(clients[nClients]->socket, &(clients[nClients]->dataBuf), 1, &recvBytes, &flags, &(clients[nClients]->overlapped), workerRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
				return 1;
			}
		}

		cout << "Socket " << acceptSocket << " got connected..." << endl;
		nClients++;
		LeaveCriticalSection(&criticalSection);
	}
	return 0;
}

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED overlapped, DWORD inFlags)
{
	DWORD sentBytes, recvBytes;
	DWORD flags;

	SocketInfo* sockInfo = (SocketInfo*)overlapped;

	if (error != 0)
		cout << "I/O operation failed with error " << error << endl;
	else if (transferredBytes == 0)
		cout << "Closing socket " << sockInfo->socket << endl << endl;
	if (error != 0 || transferredBytes == 0) {
		EnterCriticalSection(&criticalSection);

		int index;
		for (index = 0; index < nClients; index++) {
			if (clients[index]->socket == sockInfo->socket)
				break;
		}
		closesocket(clients[index]->socket);
		GlobalFree(clients[index]);
		clients[index] = 0;

		for (int i = index; i < nClients - 1; i++)
			clients[i] = clients[i + 1];
		nClients--;
		LeaveCriticalSection(&criticalSection);

		return;
	}

	if (sockInfo->operation == RECEIVE) {
		sockInfo->recvBytes = transferredBytes;
		sockInfo->sentBytes = 0;
		sockInfo->operation = SEND;
	}
	else {
		sockInfo->sentBytes += transferredBytes;
	}

	if (sockInfo->recvBytes > sockInfo->sentBytes) {
		ZeroMemory(&(sockInfo->overlapped), sizeof(WSAOVERLAPPED));
		sockInfo->dataBuf.buf = sockInfo->buffer + sockInfo->sentBytes;
		sockInfo->dataBuf.len = sockInfo->recvBytes - sockInfo->sentBytes;
		sockInfo->operation = SEND;
		if (WSASend(sockInfo->socket, &(sockInfo->dataBuf), 1, &sentBytes, 0, &(sockInfo->overlapped), workerRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				cout << "WSASend() failed with error " << WSAGetLastError() << endl;
				return;
			}
		}
	}
	else {
		sockInfo->recvBytes = 0;
		flags = 0;
		ZeroMemory(&(sockInfo->overlapped), sizeof(WSAOVERLAPPED));
		sockInfo->dataBuf.len = DATA_BUFSIZE;
		sockInfo->dataBuf.buf = sockInfo->buffer;
		sockInfo->operation = RECEIVE;
		if (WSARecv(sockInfo->socket, &(sockInfo->dataBuf), 1, &recvBytes, &flags, &(sockInfo->overlapped), workerRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
				return;
			}
		}
	}
}