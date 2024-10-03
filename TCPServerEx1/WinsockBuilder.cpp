#include "WinsockBuilder.h"
#include<iostream>
#include<string.h>
using namespace std;

WinsockBuilder::WinsockBuilder(int vers1, int vers2) {
	this->wVersion = MAKEWORD(vers1, vers2);
}

void WinsockBuilder::Startup() {
	int rc = WSAStartup(wVersion, &wsaData);
	if (rc != 0) {
		throw "WinSock is not supported";
		return;
	}
	cout << "Starting up Winsock is successful!" << endl;
}

void WinsockBuilder::Cleanup() {
	WSACleanup();
}

WinsockBuilder::~WinsockBuilder() {
	this->Cleanup();
}




