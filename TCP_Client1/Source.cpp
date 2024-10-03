#include<iostream>
#include<Winsock2.h>
#include<WS2tcpip.h>
#include<stdlib.h>
#include<string.h>
#include<string>

#pragma comment(lib, "Ws2_32.lib")

#define ENDING_DELIMITER '#'
#define FINISH_SIGN '@'
#define STATUS_FAILED "Failed"
#define STATUS_SUCCESS "Success"

#define BUFFER_SIZE 2048
using namespace std;
typedef unsigned short u_short;

void printError(const char* msg) {
	cout << "Error: " << WSAGetLastError() << " " << msg << endl;
}


void setup_sockaddr(sockaddr_in* addr, const char* ipStr, u_short port) {
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	inet_pton(AF_INET, ipStr, &addr->sin_addr);
}



int main(int argc, char** argv) {
	char serverIP[INET_ADDRSTRLEN];
	u_short serverPort;

	strcpy_s(serverIP, argv[1]);
	serverPort = atoi(argv[2]);

	WSAData wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Cannot startup Winsock2!" << endl;
		return 0;
	}

	SOCKET client;
	client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client == INVALID_SOCKET) {
		printError("Cannot create client socket");
		WSACleanup();
		return 0;
	}

	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)(&tv), sizeof(int));

	sockaddr_in serverAddr;
	setup_sockaddr(&serverAddr, serverIP, serverPort);

	// connect to server
	if (connect(client, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printError("Cannot connect to server.");
		closesocket(client);
		WSACleanup();
		return 0;
	}

	cout << "Connect to Server!" << endl;

	char buff[BUFFER_SIZE];
	int ret, messageLen;
	
	while (1) {
		cout << "Send data to server" << endl;

		// Nhap thong diep vao
		gets_s(buff, BUFFER_SIZE);
		messageLen = strlen(buff);
		

		// Xu ly thong diep gui di
		if (messageLen == 0) break;

		string mess(buff);
		for (unsigned int i = 0; i < mess.size(); i++) {
			if (mess[i] == ENDING_DELIMITER) {
				mess[i] = 'e';
			}
		}
		mess += ENDING_DELIMITER;
		strcpy_s(buff, mess.c_str());
		messageLen = strlen(buff);
		// Gui thong diep

		ret = send(client, buff, messageLen, 0);
		if (ret == SOCKET_ERROR) {
			printError("Cannot send data");
			break;
		}

		ret = recv(client, buff, BUFFER_SIZE, 0);
		if (ret == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAETIMEDOUT) {
				cout << "Time-out" << endl;
			}
			else {
				printError("Cannot receive data");
				break;
			}
		}
		else if (strlen(buff) > 0) {

			buff[ret-1] = 0;
			string arr(buff);
			int pos = arr.find(STATUS_SUCCESS);
			if (pos != string::npos) {
				cout <<"Success: " << arr.substr(pos + strlen(STATUS_SUCCESS)) << endl;
			}
			else {
				pos = arr.find(STATUS_FAILED);
				if (pos != string::npos) {
					cout << "Failed: String contains non-number character" << endl;
				}
			}
			
		}
	}
	
	closesocket(client);
	
	WSACleanup();
	return 0;
}