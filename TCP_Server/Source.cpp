#include<iostream>
#include<Winsock2.h>
#include<WS2tcpip.h>
#include<stdlib.h>
#include<string.h>
#include<deque>
#include<string>
#pragma comment(lib, "WS2_32.lib")

#define ENDING_DELIMITER '#'
#define FINISH_SIGN "@"
#define SERVER_ADDR "127.0.0.1"
#define BUFFER_SIZE 2048
#define QUEUE_SIZE 10
#define DEFAULT_PORT 5500
#define OUT_OF_BUFF -1
#define MESSAGE_NUM 1024

using namespace std;

typedef unsigned short u_short;
u_short serverPort = DEFAULT_PORT;

typedef struct {
	deque<string> _deque;
	bool isFragmented;
}MessageQueue;

void printError(const char* msg) {
	cout << "Error: " << WSAGetLastError() << " " << msg << endl;
}

void setup_sockaddr(sockaddr_in *addr, const char* ipStr, u_short port) {
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	inet_pton(AF_INET, ipStr, &addr->sin_addr);
}

void print_port_vs_ip(sockaddr_in &addr) {
	char clientIP[INET_ADDRSTRLEN];
	u_short clientPort;
	clientPort = ntohs(addr.sin_port);
	inet_ntop(AF_INET, &addr.sin_addr, clientIP, sizeof(clientIP));

	cout << "Accept incomming connection from IP: " << clientIP << " Port: " << clientPort << endl;
}

bool read_system_buffer(SOCKET& s,char* buff) {
	int ret;
	ret = recv(s, buff, BUFFER_SIZE, 0);
	if (ret == SOCKET_ERROR) {
		printError("Cannot receive data.");
		return false;
	}
	else if (ret == 0) {
		cout << "Client disconnects." << endl;
		return false;
	}
	buff[ret] = 0;
	cout << "Reading system buffer is success. Data: " <<buff<< endl;
	
	return true;
}

void pass_data_from_buffer_to_queue(char* buff, MessageQueue &q) {
	int n = strlen(buff);
	int i = 0;
	
	if (q.isFragmented == true &&!q._deque.empty()) {

		string message = q._deque.back();
		while (i < n && buff[i] != ENDING_DELIMITER) {
			message += buff[i];
			i++;
		}
		if (i == n) {
			return;
		}
		i++;
	}
	
	while(i < n) {
		string new_mess;
		while (i < n && buff[i] != ENDING_DELIMITER ) {
			new_mess += buff[i];
			i++;
		}
		q._deque.push_back(new_mess);
		i++;
	}
	if (buff[n - 1] == ENDING_DELIMITER) {
		q.isFragmented = false;
	}
	else {
		q.isFragmented = true;
	}
	
}

int caculate_message(string &mess) {
	int sum = 0;
	for (unsigned int i = 0; i < mess.size(); ++i) {
		if (mess[i] >= '0' && mess[i] <= '9') {
			sum += int(mess[i]) - int('0');
		}
		else {
			return -1;
		}
	}
	return sum;
}

bool process_batch_message(SOCKET &s, MessageQueue &q) {
	int ret, sum = 0;
	char buff[BUFFER_SIZE];
	string arr = "";
	
	// tinh toan tong chu so 
	while (!q._deque.empty()) {
		string tmp = q._deque.front();
		int sum = caculate_message(tmp);
		tmp = "";
		if (sum == -1) {
			tmp += "Failed";
			tmp += ENDING_DELIMITER;
		}
		else {
			tmp += "Success";
			tmp += to_string(sum);
			tmp += ENDING_DELIMITER;
		}

		if (arr.size() + tmp.size() >= BUFFER_SIZE) {
			break;
		}
		else {
			arr += tmp;
		}
		q._deque.pop_front();
	}
	strcpy_s(buff, arr.c_str());
	ret = send(s, buff, strlen(buff), 0);
	if (ret == SOCKET_ERROR) {
		printError("Cannot send data");
		return false;
	}
	return true;
}

bool process_all_message_in_queue(SOCKET& s, MessageQueue &q) {
	string frag = "";
	if (q.isFragmented == true&& !q._deque.empty()) {
		frag = q._deque.back();
		q._deque.pop_back();
	}
	

	while (!q._deque.empty()) {
		bool check = process_batch_message(s, q);
		if (!check) return false;
	}

	if (frag.size() != 0) {
		q._deque.push_back(frag);
	}
	return true;
}


int main(int argc, char** argv) {

	serverPort = (u_short)atoi(argv[1]);

	WSAData wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if(WSAStartup(wVersion,&wsaData)){
		cout << "Cannot startup Winsock2!" << endl;
		return 0;
	}

	// Khoi tao listen Socket
	SOCKET listenSock;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		printError("Cannot create listen socket!");
		WSACleanup();
		return 0;
	}

	sockaddr_in serverAddr;
	setup_sockaddr(&serverAddr, SERVER_ADDR, serverPort);

	// binding address with listensocket
	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printError("Cannot bind this address with listen Socket!");
		return 0;
	}

	// Lang nghe trehn listenSock
	if (listen(listenSock, QUEUE_SIZE)) {
		printError("Cannot place serversocket in state listen.");
		return 0;
	}


	// Bat dau dich vu tren server
	sockaddr_in clientAddr;
	//char buff[BUFFER_SIZE];
	/*char clientIP[INET_ADDRSTRLEN];
	u_short  clientPort;*/
	cout << "Started Server" << endl;
	int clientAddrLen = sizeof(clientAddr);

	while (1) {
		SOCKET connSock;
		char buff[BUFFER_SIZE];
		int total = 0; // bien nay luu gia tri tong chu so cua thong diep truoc neu = -1 thi thong diep do loi 
		connSock = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
		if (connSock == SOCKET_ERROR) {
			printError("Cannot permit incoming connection!");
			continue;
		}
		else {
			// In thong tin ip va port cua client dang chap nhan xu ly
			print_port_vs_ip(clientAddr);
		}
		MessageQueue q; 
		q.isFragmented = false;
		// 1 lan lap while tuong ung voi 1 lan doc thong diep 
		while (read_system_buffer(connSock,buff)) {
			pass_data_from_buffer_to_queue(buff, q);
			if (process_all_message_in_queue(connSock, q) == false) {
				break;
			}
		}

		cout << "Close connect with this client." << endl;
		closesocket(connSock);
	}

	closesocket(listenSock);
	cout << "Hello World" << endl;
	WSACleanup();
	return 0;
}