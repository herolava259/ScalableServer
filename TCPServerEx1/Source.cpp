#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<stdlib.h>
#include<string.h>
#include "stdfax.h"
#include "WinsockBuilder.cpp"

#pragma comment(lib, "Ws2_32.lib")


#define SERVER_ADDR "127.0.0.1"
#define BUFFER_SIZE 2048
#define BACK_LOG 10
using namespace std;

typedef unsigned short u_short;



int main(int argc, char** argv) {

	u_short portServer = (u_short)atoi(argv[1]);
	WinsockBuilder* builder = new WinsockBuilder(2, 2);
	TCPListenSocket* listenSock;
	TCPConnSocket* pConnSock;

	
	try {
		builder->Startup();

		listenSock = new TCPListenSocket(SERVER_ADDR, portServer, BACK_LOG);
		cout << "Server started" << endl;
		// Code lop TCPConnSocket
		// code do pt accept 
		pConnSock = listenSock->acceptConn();



		
	}
	catch (const char* msg) {
		cout << "Error: " << WSAGetLastError() << " " << msg;
		delete pConnSock;
		delete listenSock;
		delete builder;
	}

	builder->Cleanup();

	delete pConnSock;
	delete listenSock;
	delete builder;

	return 0;
}