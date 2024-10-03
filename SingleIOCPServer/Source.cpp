#include<Winsock2.h>
#include<WS2tcpip.h>
#include<windows.h>
#include<process.h>
#include<iostream>
#include<conio.h>

#define PORT 5500
#define DATA_BUFSIZE 8192
#define RECEIVE 0
#define SEND 1
#define SERVER_ADDR "127.0.0.1"
#pragma comment(lib, "Ws2_32.lib")

using namespace std;
typedef struct {
	WSAOVERLAPPED overlapped;
	WSABUF dataBuff;
	CHAR buffer[DATA_BUFSIZE];
	int bufLen;
	int recvBytes;
	int sentBytes;
	int operation;
}PER_IO_OPERATION_DATA, * LPPER_IO_OPERATION_DATA;

typedef struct {
	SOCKET socket;
}PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

unsigned __stdcall serverWorkerThread(LPVOID CompletionPortID);

int main(int argc, char** argv) {
	SOCKADDR_IN serverAddr;
	SOCKET listenSock, acceptSock;
	HANDLE completionPort;
	SYSTEM_INFO systemInfo;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	DWORD transferredBytes;
	DWORD flags;
	WSADATA wsaData;

	if (WSAStartup((2, 2), &wsaData) != 0) {
		cout << "WSAStartup() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	if ((completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
		cout << "CreateIoCompletionPort() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	GetSystemInfo(&systemInfo);
	
	for (int i = 0; i < (int)systemInfo.dwNumberOfProcessors * 2; i++) {
		if (_beginthreadex(0, 0, serverWorkerThread, (void*)completionPort, 0, 0) == 0) {
			cout << "Create thread failed with error " << WSAGetLastError() << endl;
			return 1;
		}
	}

	if ((listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		cout << "WSASocket() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
	if (bind(listenSock, (PSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "bind() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	if ((acceptSock = WSAAccept(listenSock, NULL, NULL, NULL, 0)) == SOCKET_ERROR) {
		cout << "WSAAccept() failed with error" <<WSAGetLastError()<< endl;
		return 1;
	}

	if ((perHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA))) == NULL) {
		cout << "GloabalAlloc() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	cout << "Socket number " << acceptSock << "got connected..." << endl;
	perHandleData->socket = acceptSock;
	if (CreateIoCompletionPort((HANDLE)acceptSock, completionPort, (DWORD)perHandleData, 0) == NULL) {
		cout << "CreateIoCompletionPort() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	if ((perIoData = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATION_DATA))) == NULL) {
		cout << "GlobalAlloc() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
	perIoData->sentBytes = 0;
	perIoData->recvBytes = 0;
	perIoData->dataBuff.len = DATA_BUFSIZE;
	perIoData->dataBuff.buf = perIoData->buffer;
	perIoData->operation = RECEIVE;
	flags = 0;

	if (WSARecv(acceptSock, &(perIoData->dataBuff), 1, &transferredBytes, &flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
			return 1;
		}
	}
	_getch();
	return 0;
}

unsigned __stdcall serverWorkerThread(LPVOID completionPortID) {
	HANDLE completionPort = (HANDLE)completionPortID;
	DWORD transferredBytes;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	DWORD flags;

	while (TRUE) {
		if (GetQueuedCompletionStatus(completionPort, &transferredBytes, (LPDWORD)&perHandleData, (LPOVERLAPPED*)&perIoData, INFINITE) == 0) {
			cout << "GetQueuedCompletionStatus() failed with error " << WSAGetLastError() << endl;
			return 0;
		}

		if (transferredBytes == 0) {
			cout << "Closing socket " << perHandleData->socket << endl;
			if (closesocket(perHandleData->socket) == SOCKET_ERROR) {
				cout << "closesocket() failed with error " << WSAGetLastError() << endl;
				return 0;
			}
			GlobalFree(perHandleData);
			GlobalFree(perIoData);
			continue;
		}

		if (perIoData->operation == RECEIVE) {
			perIoData->recvBytes = transferredBytes;
			perIoData->sentBytes = 0;
			perIoData->operation = SEND;
		}
		else if (perIoData->operation == SEND) {
			perIoData->sentBytes += transferredBytes;
		}
		if (perIoData->recvBytes > perIoData->sentBytes) {
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->dataBuff.buf = perIoData->buffer + perIoData->sentBytes;
			perIoData->dataBuff.len = perIoData->recvBytes - perIoData->sentBytes;
			perIoData->operation = SEND;

			if (WSASend(perHandleData->socket, &(perIoData->dataBuff), 1, &transferredBytes, 0, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					cout << "WSASend() failed with error " << WSAGetLastError() << endl;
					return 0;
				}
			}

		}
		else {
			perIoData->recvBytes = 0;
			perIoData->operation = RECEIVE;
			flags = 0;
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->dataBuff.len = DATA_BUFSIZE;
			perIoData->dataBuff.buf = perIoData->buffer;
			if (WSARecv(perHandleData->socket, &(perIoData->dataBuff), 1, &transferredBytes, &flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
					return 0;
				}
			}
		}
	}
}