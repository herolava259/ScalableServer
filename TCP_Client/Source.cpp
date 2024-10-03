#include<iostream>
#include<string.h>
#include<stdlib.h>
#include<winsock2.h>
#include<WS2tcpip.h>

#define SERVER_PORT 5500
#define SERVER_ADDR "127.0.0.1"
#define BUFF_SIZE 2048
#pragma comment(lib,"Ws2_32.lib")

using namespace std;

int main(int argc, char argv[]) {

	WSAData wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Winsock 2.2 is not supported" << endl;
		return 0;
	}

	SOCKET client;
	client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (client == SOCKET_ERROR) {
		cout << "Error: " << WSAGetLastError() << "Cannot create server socket" << endl;
		return 0;
	}

	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (connect(client, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error: " << WSAGetLastError() << " Cannot connect to Server" << endl;
		return 0;
	}

	cout << "Connected Server!" << endl;
	char buff[BUFF_SIZE];
	int ret, messageLen;

	while (1) {
		cout << "Send to Server!" << endl;
		gets_s(buff, BUFF_SIZE);
		messageLen = strlen(buff);
		if (messageLen == 0) {
			break;
		}
		ret = send(client, buff, messageLen, 0);

		if (ret == SOCKET_ERROR) {
			cout << "Error: " << WSAGetLastError() << "Cannot send data" << endl;
			
		}

		ret = recv(client, buff, BUFF_SIZE, 0);
		if (ret == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAETIMEDOUT) {
				cout << "TIME OUT" << endl;
			}
			else {
				cout << "Error: " << WSAGetLastError() << endl;
			}
		}
		else if (strlen(buff) > 0) {
			buff[ret] = 0;
			cout << "Receive from server: " << buff << endl;
		}
	}
	closesocket(client);

	WSACleanup();

	return 0;
}