#include"stdafx.h"
#include <string.h>
#include<iostream>
#include<winsock2.h>
#include<WS2tcpip.h>

#define SERVER_PORT 5500
#define SERVER_ADDR "127.0.0.1"
#define BUFF_SIZE 2048
#define MAX_CLIENT 3
#pragma comment(lib, "Ws2_32.lib")
using namespace std;
/*
* Che do chan dung tren window 
*  + Cac thao tac vao ra tren SOCKET se tro ve noi goi cua no ngay lap tuc
*  + Ket qua cua thao tac vao ra se duoc thong bao tren 1 co che nao do 
*  + Cac ham bat dong bo se tra ve ma lopi WSAEWOULDBLOCK neu thao tac vao ra khong the hoan tat ngay va mat thoi gian dang ke
*  + Socket chuyen sang che do nay bang ham ioctlsocket()
*  + int ioctlsocket ( SOCKET s, long cmd, u_long *argp)
*   parameter: 
*    - SOCKET s: socket can chuyen sang che do chan dung 
*    - cmd : che do dieu khien vao ra
*    - *argp : con tro tro den noi thiet lap gia tri cho cmd
*/

int main() {

	WSAData wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Winsock2.2 is not supported" << endl;
		return 0;
	}
	SOCKET listenSock;
	unsigned long ul = 1;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		cout << "Cannot create server socket" << endl;
		WSACleanup();
		return 0;
	}
	if (ioctlsocket(listenSock, FIONBIO, (u_long*)&ul)) {
		cout << "Cannot change socket to non-blocking state" << endl;
		return 0;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error: " << WSAGetLastError() << " Cannot bind server socker with address." << endl;
		WSACleanup();
		return 0;
	}

	if (listen(listenSock, 10)) {
		cout << "Error: " << WSAGetLastError() << " Cannot switch socket to listen state" << endl;
		return 0;
	}
	cout << "Server started!" << endl;
	// bat dau khoi tao

	sockaddr_in clientAddr;
	char buff[BUFF_SIZE];
	SOCKET client[MAX_CLIENT];
	int i, ret, clientAddrLen = sizeof(clientAddr);
	for (i = 0; i < MAX_CLIENT; ++i) {
		client[i] = 0;
	}

	while (1) {
		SOCKET connSock;
		connSock = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
		if (connSock != SOCKET_ERROR) {
			for (i = 0; i < MAX_CLIENT; ++i) {
				if (client[i] == 0) {
					client[i] = connSock;
					break;
				}
			}

			if (i == MAX_CLIENT) {
				cout << "Cannot response any client." << endl;
				closesocket(connSock);
			}
		}

		int errorCode;


		for (i = 0; i < MAX_CLIENT; ++i) {
			if (client[i] == 0) continue;

			ret = recv(client[i], buff, BUFF_SIZE, 0);
			if (ret == SOCKET_ERROR) {
				errorCode = WSAGetLastError();
				if (errorCode == WSAEWOULDBLOCK) {
					cout << "Cannot receive data" << endl;
					closesocket(client[i]);
					client[i] = 0;
				}
			}
			else if (ret == 0) {
				cout << "Client dosconnect!" << endl;
				closesocket(client[i]);
				client[i] = 0;
			}
			else {
				buff[ret] = 0;
				char addr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &clientAddr.sin_addr, addr, sizeof(addr));
				cout << "Receivet data from client" << i << "with address: " <<addr <<" and port: " << ntohs(clientAddr.sin_port) << "and data: " << buff << endl;

				ret = send(client[i], buff, strlen(buff), 0);
				if (ret == SOCKET_ERROR) {
					errorCode = WSAGetLastError();
					if (errorCode == WSAEWOULDBLOCK) {
						cout << "Cannot send data to client " << i << endl;
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