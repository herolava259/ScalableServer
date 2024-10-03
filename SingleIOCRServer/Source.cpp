#include<Winsock2.h>
#include<windows.h>
#include<iostream>
#include<WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_ADDR "127.0.0.1"
#define PORT 5500
#define DATA_BUFSIZE 8192
#define RECEIVE 0
#define SEND 1

using namespace std;

SOCKET acceptSocket;
WSABUF dataBuf;
char buffer[DATA_BUFSIZE];
int operation;

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED lpOverlapped, DWORD inFlags);

int main(int argc, char* argv[])
{
	DWORD flags, recvBytes, index;
	WSADATA wsaData;
	SOCKADDR_IN serverAddr, clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	WSAEVENT events[1];
	SOCKET listenSocket;
	WSAOVERLAPPED overlapped;

	if (WSAStartup((2, 2), &wsaData)) {
		cout << "WSAstartup() failed with error " << WSAGetLastError() << endl;
		WSACleanup();
		return 1;
	}

	if ((listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		cout << "Failed to get a socket " << WSAGetLastError() << endl;
		return 1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSocket, (PSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "bind() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	if (listen(listenSocket, 5)) {
		cout << "listen() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	cout << "Server started!" << endl;

	if ((events[0] = WSACreateEvent()) == WSA_INVALID_EVENT) {
		cout << "WSACreateEvent() failed with error " << WSAGetLastError() << endl;
		
		return 1;
	}

	if ((acceptSocket = accept(listenSocket, (PSOCKADDR)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
		cout << "accept() failed with error" << WSAGetLastError() << endl;
		return 1;
	}

	ZeroMemory(&overlapped, sizeof(WSAOVERLAPPED));
	operation = RECEIVE;
	dataBuf.len = DATA_BUFSIZE;
	dataBuf.buf = buffer;
	flags = 0;

	if (WSARecv(acceptSocket, &dataBuf, 1, &recvBytes, &flags, &overlapped, workerRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
			return 1;
		}
	}

	while (1) {
		index = WSAWaitForMultipleEvents(1, events, FALSE, WSA_INFINITE, TRUE);
		if (index == WSA_WAIT_FAILED) {
			cout << "WSAWaitForMultipleEvents() failed with error " << WSAGetLastError() << endl;
			return 1;
		}

		if (index != WAIT_IO_COMPLETION)
			break;
	}
	WSAResetEvent(events[index - WSA_WAIT_EVENT_0]);
	return 0;
}

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED lpOverlapped, DWORD inFlags) {
	DWORD sentBytes = 0, recvBytes = 0;
	DWORD flags = 0;
	if (error != 0)
		cout << "I/O operation failed with error " << error << endl;
	if (transferredBytes == 0)
		cout << "Connection closed" << endl;

	if (error != 0 || transferredBytes == 0) {
		closesocket(acceptSocket);
		return;
	}

	if (operation == RECEIVE) {
		recvBytes = transferredBytes;
		sentBytes = 0;
		operation = SEND;
	}
	else {
		sentBytes += transferredBytes;
	}

	if (recvBytes > sentBytes) {
		dataBuf.buf = buffer + sentBytes;
		dataBuf.len = recvBytes - sentBytes;
		operation = SEND;

		if (WSASend(acceptSocket, &dataBuf, 1, &transferredBytes, flags, lpOverlapped, workerRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				cout << "WSASend() failed with error " << WSAGetLastError() << endl;
				closesocket(acceptSocket);
				return;
			}
		}
	}
	else {
		ZeroMemory(lpOverlapped, sizeof(WSAOVERLAPPED));
		recvBytes = 0;
		operation = RECEIVE;
		dataBuf.len = DATA_BUFSIZE;
		dataBuf.buf = buffer;
		flags = 0;

		if (WSARecv(acceptSocket, &dataBuf, 1, &transferredBytes, &flags, lpOverlapped, workerRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				cout << "WSARecv() failed with error " << WSAGetLastError() << endl;

				closesocket(acceptSocket);
				return;
			}
		}
	}

}

