#include<Winsock2.h>
#include<iostream>
#include<WS2tcpip.h>
#include<string.h>

#define SERVER_ADDR "127.0.0.1"
#define PORT 5500
#define DATA_BUFSIZE 8192
#define RECEIVE 0
#define SEND 1

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

typedef struct{
	SOCKET socket;
	WSAOVERLAPPED overlapped;
	WSABUF dataBuf;
	char buffer[DATA_BUFSIZE];
	int operation;
	int sentBytes;
	int recvBytes;
} SocketInfo;

void freeSockInfo(SocketInfo* siArray[], int n);

void closeEventInArray(WSAEVENT enventArr[], int n);

int main()
{
	SocketInfo* socks[WSA_MAXIMUM_WAIT_EVENTS];
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	int nEvents = 0;

	WSADATA wsaData;
	if (WSAStartup((2, 2), &wsaData) != 0) {
		cout << "WSAStartup() failed with error " << WSAGetLastError() << endl;
	}

	for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		socks[i] = 0;
		events[i] = 0;
	}

	SOCKET listenSock;
	if ((listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		cout << "Error " << WSAGetLastError() << ": Cannot create server socket." << endl;
		return 1;
	}

	events[0] = WSACreateEvent();
	nEvents++;
	WSAEventSelect(listenSock, events[0], FD_ACCEPT | FD_CLOSE);

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error " << WSAGetLastError() << ": Cannot associate a local address with server socket." << endl;
		return 0;
	}

	if (listen(listenSock, 10)) {
		cout << "Error " << WSAGetLastError() << ": Cannot place server socket in state LISTEN." << WSAGetLastError() << endl;
		return 0;
	}

	cout << "Server started!" << endl;

	int index; 
	SOCKET connSock;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	while (1) {
		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);

		if (index == WSA_WAIT_FAILED) {
			cout << "Error " << WSAGetLastError() << " : WSAWaitForMultipleEvents() failed " << endl;
			return 0;
		}

		index = index - WSA_WAIT_EVENT_0;

		DWORD flags, transferredBytes;

		if (index == 0) {
			WSAResetEvent(events[0]);
			if ((connSock = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen)) == INVALID_SOCKET) {
				cout << "Error " << WSAGetLastError() << "Cannot permit comming connection." << endl;
				return 0;
			}

			int i;
			if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
				cout << "Too many clients." << endl;
				closesocket(connSock);
			}
			else {
				WSAEventSelect(connSock, NULL, 0);

				i = nEvents;
				events[i] = WSACreateEvent();
				socks[i] = (SocketInfo *)malloc(sizeof(SocketInfo));
				socks[i]->socket = connSock;
				memset(&socks[i]->overlapped, 0, sizeof(WSAOVERLAPPED));
				socks[i]->overlapped.hEvent = events[i];
				socks[i]->dataBuf.buf = socks[i]->buffer;
				socks[i]->operation = RECEIVE;
				socks[i]->recvBytes = 0;
				socks[i]->sentBytes = 0;

				nEvents++;

				flags = 0;
				if (WSARecv(socks[i]->socket, &(socks[i]->dataBuf), 1, &transferredBytes, &flags, &(socks[i]->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
						closeEventInArray(events, i);
						freeSockInfo(socks, i);
						nEvents--;
					}
				}
			}
		}
		else {
			SocketInfo* client;
			client = socks[index];
			WSAResetEvent(events[index]);
			BOOL result;
			result = WSAGetOverlappedResult(client->socket, &(client->overlapped), &transferredBytes, FALSE, &flags);
			if (result == FALSE || transferredBytes == 0) {
				closesocket(client->socket);
				closeEventInArray(events, index);
				freeSockInfo(socks, index);
				client = 0;
				nEvents--;
				continue;
			}

			if (client->operation == RECEIVE) {
				client->recvBytes = transferredBytes;
				client->sentBytes = 0;
				client->operation = SEND;
			}
			else {
				client->sentBytes += transferredBytes;
				
			}

			if (client->recvBytes > client->sentBytes) {
				client->dataBuf.buf = client->buffer + client->sentBytes;
				client->dataBuf.len = client->recvBytes - client->sentBytes;
				client->operation = SEND;
				if (WSASend(client->socket, &(client->dataBuf), 1, &transferredBytes, flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						cout << "WSASend() failed with error " << WSAGetLastError() << endl;
						closesocket(client->socket);
						closeEventInArray(events, index);
						freeSockInfo(socks, index);
						client = 0;
						nEvents--;
						continue;
					}
				}
			}
			else {
				memset(&(client->overlapped), 0, sizeof(WSAOVERLAPPED));
				client->overlapped.hEvent = events[index];
				client->recvBytes = 0;
				client->operation = RECEIVE;
				client->dataBuf.buf = client->buffer;
				client->dataBuf.len = DATA_BUFSIZE;
				flags = 0;

				if (WSARecv(client->socket, &(client->dataBuf), 1, &transferredBytes, &flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						cout << "WSARecv failed with error " << WSAGetLastError() << endl;
						closesocket(client->socket);
						closeEventInArray(events, index);
						freeSockInfo(socks, index);
						client = 0;
						nEvents--;
					}
				}
			}

		}
	}
}

void freeSockInfo(SocketInfo* siArray[], int n) {
	closesocket(siArray[n]->socket);
	free(siArray[n]);
	for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
		siArray[i] = siArray[i + 1];
	}
}

void closeEventInArray(WSAEVENT eventArr[], int n) {
	WSACloseEvent(eventArr[n]);

	for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++)
		eventArr[i] = eventArr[i + 1];
}