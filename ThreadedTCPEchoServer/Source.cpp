#include<iostream>
#include<string.h>
#include<Winsock2.h>
#include<WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 5500
#define SERVER_ADDR "127.0.0.1"
#define BUFFER_SIZE 2048
#define MAX_CLIENT 3

using namespace std;

int main() {
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Winsock 2 is not supported!" << endl;
		return 0;
	}

	SOCKET listenSock;
	unsigned long ul = 1;

	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		cout << "Error: " << WSAGetLastError() << " Cannot create server socket!" << endl;
		WSACleanup();
		return 0;
	}

	if (ioctlsocket(listenSock, FIONBIO, (unsigned long*)&ul)) {
		cout << "Cannot change to non-blocking mode." << endl;
		return 0;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error: " << WSAGetLastError() << " Cannot bind listenSock with this address." << endl;
		return 0;
	}

	if (listen(listenSock, 10)) {
		cout << "Error: " << WSAGetLastError() << " Cannot listen at this socket" << endl;
		return 0;
	}

	cout << "Server started!" << endl;
	sockaddr_in clientAddr;
	char buff[BUFFER_SIZE];
	int i, ret, clientAddrLen = sizeof(clientAddr);
	SOCKET client[MAX_CLIENT];

	for (int i = 0; i < MAX_CLIENT; i++) {
		client[i] = 0;
	}

	while (1) {
		SOCKET connSock;
		connSock = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
		if (connSock != SOCKET_ERROR) {
			for (i = 0; i < MAX_CLIENT; i++) {
				if (client[i] == 0) {
					client[i] = connSock;
					break;
				}
			}
			if (i == MAX_CLIENT) {
				cout << "Error: Cannot response any client." << endl;
				closesocket(connSock);
			}
		}
		int errorCode;
		for (int i = 0; i < MAX_CLIENT; i++) {
			if (client[i] == 0) continue;
			ret = recv(client[i], buff, BUFFER_SIZE, 0);
			if (ret == SOCKET_ERROR) {
				errorCode = WSAGetLastError();
				if (errorCode != WSAEWOULDBLOCK) {
					cout << "Error: " << errorCode << " Cannot receive data from client " << i << endl;
					closesocket(client[i]);
					client[i] = 0;

				}
			}
			else if (ret == 0) {
				cout << "Client " << i << " connects" << endl;
				closesocket(client[i]);
				client[i] = 0;
			}
			else {
				buff[ret] = 0;
				cout << "Received data from client " << i << " " << buff << endl;

				ret = send(client[i], buff, strlen(buff), 0);
				if (ret == SOCKET_ERROR) {
					errorCode = WSAGetLastError();
					if (errorCode != WSAEWOULDBLOCK) {
						cout << "Error: " << errorCode << " Cannot send data to client " << i << endl;
						closesocket(client[i]);
						client[i] = 0;

					}
				}
			}
		}
	}

	closesocket(listenSock);
	WSACleanup();
	return 0;
}