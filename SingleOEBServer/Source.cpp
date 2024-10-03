#include<Winsock2.h>
#include<windows.h>
#include<iostream>
#include<WS2tcpip.h>

#define PORT 5500
#define SERVER_ADDR "127.0.0.1"
#define DATA_BUFSIZE 8192
#define RECEIVE 0
#define SEND 1



#pragma comment(lib, "Ws2_32.lib")
using namespace std;

int main(int argc, char** argv) {
	SOCKET connSocket, listenSocket;
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	DWORD flags = 0, nEvents = 0, recvBytes = 0, transferredBytes = 0, sentBytes = 0;
	int operation;
	WSAOVERLAPPED acceptOverlapped;
	WSABUF dataBuf;
	char buffer[DATA_BUFSIZE];
	
	SOCKADDR_IN serverAddr, clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	int ret;

	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Winsock 2.2 is not supported" << endl;
		return 0;
	}

	if ((listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		cout << "Error" << WSAGetLastError() << ": Cannot create server socket" << endl;
		return 1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
	if (bind(listenSocket, (PSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "Error " << WSAGetLastError() << ": Cannot associate a local address with server socket." << endl;
		return 1;
	}

	if (listen(listenSocket, 5)) {
		cout << "Error " << WSAGetLastError() << " : Cannot place server socket in state LISTEN." << endl;
		return 1;
	}

	cout << "Server start!" << endl;

	if ((events[0] = WSACreateEvent()) == WSA_INVALID_EVENT) {
		cout << "WSACreateEvent() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	nEvents++;

	if ((connSocket = accept(listenSocket, (PSOCKADDR)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
		cout << "accept() failed with error " << WSAGetLastError() ;
		return 1;
	}
	events[nEvents] = WSACreateEvent();
	ZeroMemory(&acceptOverlapped, sizeof(WSAOVERLAPPED));
	acceptOverlapped.hEvent = events[nEvents];
	nEvents++;

	operation = RECEIVE;
	dataBuf.len = DATA_BUFSIZE;
	dataBuf.buf = buffer;
	if (WSARecv(connSocket, &dataBuf, 1, &transferredBytes, &flags, &acceptOverlapped, NULL) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
			return 1;
		}
	}
	if (WSASetEvent(events[0]) == FALSE) {
		cout << "WSASetEvent() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	while (1) {

		DWORD index;

		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
		index = index - WSA_WAIT_EVENT_0;

		if (index == 0) {
			WSAResetEvent(events[0]);
			continue;
		}

		WSAGetOverlappedResult(connSocket, &acceptOverlapped, &transferredBytes, FALSE, &flags);

		WSAResetEvent(events[index]);

		if (transferredBytes == 0) {
			cout << "CLosing socket " << connSocket << endl;
			closesocket(connSocket);
			WSACloseEvent(events[index]);
			return 1;
		}

		if (operation == RECEIVE) {
			recvBytes = transferredBytes;
			sentBytes = 0;
			operation = SEND;
		}
		else {
			sentBytes += transferredBytes;
			if (recvBytes > sentBytes) {
				dataBuf.buf = buffer + sentBytes;
				dataBuf.len = recvBytes - sentBytes;
				operation = SEND;

				if (WSASend(connSocket, &dataBuf, 1, &transferredBytes, flags, &acceptOverlapped, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						cout << "WSASend() failed with error " << WSAGetLastError() << endl;
						closesocket(connSocket);
						return 1;
					}
				}
			}
			else {
				ZeroMemory(&acceptOverlapped, sizeof(WSAOVERLAPPED));
				acceptOverlapped.hEvent = events[index];
				recvBytes = 0;
				operation = RECEIVE;
				dataBuf.len = DATA_BUFSIZE;
				dataBuf.buf = buffer;
				flags = 0;

				if (WSARecv(connSocket, &dataBuf, 1, &transferredBytes, &flags, &acceptOverlapped, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
						closesocket(connSocket);
						return 1;
					}
				}
			}
		}
	}
	
	closesocket(listenSocket);
	WSACleanup();
	return 0;
}