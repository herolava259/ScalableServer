#include<iostream>
#include<Winsock2.h>
#include<WS2tcpip.h>
#include<string.h>
#pragma comment(lib, "Ws2_32.lib")

#define PORT 5500
#define SERVER_ADDR "127.0.0.1"
#define BUFFER_SIZE 2048
int Receive(SOCKET, char*, int, int);
int Send(SOCKET, char*, int, int);
using namespace std;

int main(int argc, char** argv) {
	DWORD nEvents = 0;
	DWORD index;
	SOCKET socks[WSA_MAXIMUM_WAIT_EVENTS];
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	WSANETWORKEVENTS sockEvent;

	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);

	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Winsock 2.2 is not supported" << endl;
		return 0;
	}

	SOCKET listenSock;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		cout << "Cannot create listen socket in server" << endl;
		return 0;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	socks[0] = listenSock;
	events[0] = WSACreateEvent();
	nEvents++;

	WSAEventSelect(socks[0], events[0], FD_ACCEPT | FD_CLOSE);
	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error: " << WSAGetLastError()<<"Cannot associate a local address with server socket" << endl;
		return 0;
	}
	if (listen(listenSock, 10)) {
		cout << "Error: " << WSAGetLastError() << "Cannot associate a local address with serversocket" << endl;
		return 0;
	}

	char sendBuff[BUFFER_SIZE], recvBuff[BUFFER_SIZE];
	SOCKET connSock;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	int ret, i;
	for (i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; ++i) {
		socks[i] = 0;
	}

	while (1) {
		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) {
			cout << "Error: " << WSAGetLastError() << "WSAWaitForMultipleEvents() failed" << endl;
			break;
		}
		index = index - WSA_WAIT_EVENT_0;
		WSAEnumNetworkEvents(socks[index],events[index],&sockEvent);

		if (sockEvent.lNetworkEvents & FD_ACCEPT) {
			if (sockEvent.iErrorCode[FD_ACCEPT_BIT] != 0) {
				cout << "FD_ACCEPT failed with error code " << sockEvent.iErrorCode[FD_READ_BIT] << endl;
				break;
			}
			if ((connSock = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
				cout << "Error: " << WSAGetLastError() << "Cannot permit incoming connection" << endl;
				break;
			}


			if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
				cout << "Too many clients." << endl;
				closesocket(connSock);
			}
			else {
				for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
					if (socks[i] == 0) {
						socks[i] = connSock;
						events[i] = WSACreateEvent();
						WSAEventSelect(socks[i], events[i], FD_READ | FD_CLOSE);
						nEvents++;
						break;
					}
				}
			}
			WSAResetEvent(events[index]);
		}

		if (sockEvent.lNetworkEvents & FD_READ) {
			if (sockEvent.iErrorCode[FD_READ_BIT] != 0) {
				cout << "FD_READ failed with error: " << sockEvent.iErrorCode[FD_READ_BIT] << endl;
				break;
			}

			ret = Receive(socks[index], recvBuff, BUFFER_SIZE, 0);

			if (ret <= 0) {
				closesocket(socks[index]);
				socks[index] = 0;
				WSACloseEvent(events[index]);
				nEvents--;
			}
			else {
				memcpy(sendBuff, recvBuff, ret);
				Send(socks[index], sendBuff, ret, 0);
				WSAResetEvent(events[index]);
			}
		}

		if (sockEvent.lNetworkEvents & FD_CLOSE) {
			if (sockEvent.iErrorCode[FD_CLOSE_BIT] != 0) {
				cout << "FD_CLOSE failed with error " << sockEvent.iErrorCode[FD_CLOSE_BIT] << endl;
				break;
			}
			closesocket(socks[index]);
			socks[index] = 0;
			WSACloseEvent(events[index]);
			nEvents--;
		}
		
	}
	closesocket(listenSock);
	WSACleanup();
	return 0;
}

int Receive(SOCKET s, char* buff, int size, int flags) {
	int n;

	n = recv(s, buff, size, flags);
	if (n == SOCKET_ERROR) {
		cout << "Error " << WSAGetLastError() << ": Cannot receive data" << endl;

	}
	else if (n == 0) {
		cout << "Client disconnects" << endl;
	}
	return n;
}

int Send(SOCKET s, char* buff, int size, int flags) {
	int n;

	n = send(s, buff, size, flags);
	if (n == SOCKET_ERROR) {
		cout << "Error " << WSAGetLastError() << ": Cannot send data." << endl;
	}

	return n;
}