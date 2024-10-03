#include<Winsock2.h>
#include<Windows.h>
#include<WS2tcpip.h>

#include<sstream>
#include<fstream>
#include<string>
#include<iostream>
#include<process.h>

#include<stdlib.h>
#include<time.h>

#include<stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define DATA_BUFSIZE 8192
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define MAX_CLIENT 1024
#define BUFF_SIZE 2048
#define PORT 5500

#define ENC_CODE 0
#define DEC_CODE 1
#define TRANS_CODE 2
#define ERROR_CODE 3

#define DEFAULT_CODE 4
#define LENGTH_NAME 7

#define INIT_TYPE -1
#define ENC_TYPE 0
#define DEC_TYPE 1

#define INIT_STATE 0
#define READ_STATE 1
#define WRITE_STATE 2
#define FINISH_STATE 3
#define PENDING_STATE 4

#define NORMAL_STATUS 0
#define SHUTDOWN_STATUS 1

#define OPCODE_SIZE 1
#define LENGTH_FIELD_SIZE 4

#define LINIT_INTERGER 0xffffffff
#define LIMIT_TRY_NUM 10

#define RECEIVE 0
#define SEND 1

#define SEND_KEY 0
#define RECEIVE_KEY 1
#define SHUTDOWN_KEY 2

using namespace std;

typedef struct {
	char opcode;
	char key;
	unsigned int length;
	string fileName;
} InitedData;

typedef struct {
	char opcode;
	unsigned int length;
}DataHeader;
typedef struct {
	WSAOVERLAPPED overlapped;
	SOCKET socket;
	WSABUF dataBuf;
	char buffer[DATA_BUFSIZE];
	ApplicationInterface* pApp;
	int operation;
	int sentBytes;
	int recvBytes;
}SocketInfo;
class Utils {
private:
	static Utils* pInstance;
	Utils() {
	}
public:
	Utils* getInstance() {
		if (pInstance == nullptr) {
			pInstance = new Utils();
		}
		return pInstance;
	}
	static string createRandomNameFile(unsigned int rnd_seed) {
		srand(rnd_seed);
		string name = "";
		char offset = 0;
		for (int i = 0; i < LENGTH_NAME; i++) {

			switch (rand() % 3) {
			case 0:
			{
				offset = rand() % 10;
				name += char('0' + offset);
				break;
			}
			case 1:
			{
				offset = rand() % 26;
				name += char('a' + offset);
				break;
			}
			case 2:
			{
				offset = rand() % 26;
				name += char('A' + offset);
				break;
			}
			}
		}
		name += ".txt";
		return name;
	}

	static unsigned int byteArr_to_interger(char* first) {
		unsigned int res = 0;
		unsigned int tmp = 0;
		tmp = *first;
		res |= tmp << (32 - 8);
		tmp = *(first + 1);
		res |= tmp << (24 - 8);
		tmp = *(first + 2);
		res |= tmp << (16 - 8);
		tmp = *(first + 3);
		res |= tmp;
		return res;
	}

	static void interger_to_byteArr(unsigned int num, char* buff) {
		*(buff + 3) = (char)num;
		*(buff + 2) = (char)(num >> 8);
		*(buff + 1) = (char)(num >> 16);
		*buff = (char)(num >> 24);
	}

	static char encode(char letter, char key, int type) {
		if (type == ENC_TYPE) {
			return (char)((letter + (int)key) % 256);
		}
		else if (type == DEC_TYPE) {
			return (char)((letter - (int)key) % 256);
		}
		return 0;
	}

	static bool write_data_to_file(fstream& stream, char* dat, int length) {

		for (int i = 0; i < length; i++) {
			stream.put(*(dat + i));
			if (0) {
				return false;
			}
		}
		return true;
	}

	static int concatData(char* dst, int lenDst, char* src, int lenSrc) {
		for (int i = lenDst; i < lenDst + lenSrc; i++) {
			dst[i] = src[i - lenDst];
		}
		dst[lenDst + lenSrc] = 0;
		return lenDst + lenSrc;
	}

	static int shift_bytes_to_first(char* arr, int len, int shift_idx) {
		int i = 0;
		for (int i = 0; i < len - shift_idx; i++) {
			arr[i] = arr[i + shift_idx];

		}
		return i;
	}

	static int read_data_from_file(fstream& stream, char* buff, int length) {
		char c = 0;
		int i = 0;
		while (stream.get(c) && i < length) {
			buff[i] = c;
			i++;
			if (0) {
				return -1;
			}
		}
		return i;
	}

	static DataHeader retrieveHeader(char* dat) {
		DataHeader header;
		header.opcode = dat[0];
		header.length = byteArr_to_interger(dat + 1);
		return header;
	}

	static InitedData init_session_with_first_data(char* dat, int rnd_seed) {
		InitedData pack;
		pack.fileName = createRandomNameFile((unsigned int)time(0) + rnd_seed);
		pack.opcode = dat[0];
		pack.length = byteArr_to_interger(dat + 1);
		pack.key = dat[OPCODE_SIZE + LENGTH_FIELD_SIZE];
		return pack;
	}
};
SOCKET acceptSocket;
SocketInfo* clients[MAX_CLIENT];
int nClients = 0;
CRITICAL_SECTION criticalSection;
u_short serverPort = 0;



class ApplicationInterface {
public:
	ApplicationInterface()
	{}

	virtual bool forwardData(char* inp_dat, int length) {
		return 1;
	}

	virtual bool retrieveData(char* out_dat, int* length) {
		return 1;
	}

	virtual bool processData() {
		return 1;
	}

	virtual int getCtrlKey() {
		return 0;
	}
};


class EncDecSession : public ApplicationInterface
{
private:
	int index;
	int type;
	int state;
	int status;
	char key;

	unsigned long offsetFile;
	unsigned int packageLength;
	unsigned long fileLength;
	char pendingBytes[OPCODE_SIZE + LENGTH_FIELD_SIZE + 1];
	string fileName;

public:
	EncDecSession():ApplicationInterface() {
		index = 0;
		type = INIT_TYPE;
		state = INIT_TYPE;
		key = 0;
		status = 0;
		offsetFile = 0;
	}
};
// Viet tiop EncDecSession
unsigned __stdcall ioThread(LPVOID lpParameter)
{
	DWORD flags;
	WSAEVENT events[1];
	DWORD index;
	DWORD recvBytes;

	events[0] = (WSAEVENT)lpParameter;

	while (TRUE)
	{
		while (TRUE) {
			index = WSAWaitForMultipleEvents(1, events, FALSE, WSA_INFINITE, TRUE);

			if (index == WSA_WAIT_FAILED)
			{
				cout << "WSAWaitForMultipleEvents() failed with error " << WSAGetLastError() << endl;
				return 1;
			}

			if (index != WAIT_IO_COMPLETION) {
				break;
			}
		}

		WSAResetEvent(events[index - WSA_WAIT_EVENT_0]);

		EnterCriticalSection(&criticalSection);

		if (nClients == MAX_CLIENT) {
			cout << "TOO many clients " << endl;
			continue;
		}

		if ((clients[nClients] = (SocketInfo*)GlobalAlloc(GPTR, sizeof(SocketInfo))) == NULL) {
			cout << "GlobalAlloc() failed with error " << GetLastError() << endl;
			return 1;
		}

		clients[nClients]->socket = acceptSocket;
		memset(&clients[nClients]->overlapped, 0, sizeof(WSAOVERLAPPED));
		clients[nClients]->sentBytes = 0;
		clients[nClients]->recvBytes = 0;
		clients[nClients]->dataBuf.len = DATA_BUFSIZE;
		clients[nClients]->dataBuf.buf = clients[nClients]->buffer;
		clients[nClients]->operation = RECEIVE;
		if ((clients[nClients]->pApp = new ApplicationInterface()) == NULL){
			cout << "Cannot create ApplicationInterface " << GetLastError() << endl;
			return 0;
		}
		flags = 0;

		if (WSARecv(clients[nClients]->socket, &(clients[nClients]->dataBuf), 1, &recvBytes, &flags, &(clients[nClients]->overlapped), workerRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING) {
				cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
				return 1;
			}
		}
		cout << "Socket " << acceptSocket << " got connected... " << endl;
		nClients++;
		LeaveCriticalSection(&criticalSection);
	}
	return 0;
}

void freeSessionResource(SocketInfo* pInfo) {
	EnterCriticalSection(&criticalSection);
	int index = 0;
	for (index = 0; index < nClients; index++) {
		if (clients[index] == pInfo)
			break;
	}
	closesocket(clients[index]->socket);
	delete clients[index]->pApp;
	GlobalFree(clients[index]);
	clients[index] = 0;

	for (; index < nClients; index++) {
		clients[index] = clients[index + 1];
	}
	nClients--;
	LeaveCriticalSection(&criticalSection);
	return;
}

void CALLBACK workerRoutine(DWORD error, DWORD transferredBytes, LPWSAOVERLAPPED overlapped, DWORD inFlags) {
	DWORD sentBytes, recvBytes;
	DWORD flags;

	SocketInfo* sockInfo = (SocketInfo*)overlapped;

	if (error != 0)
		cout << "I/O operation failed with error " << error << endl;
	else if (transferredBytes == 0)
		cout << "Closing socket " << sockInfo->socket << endl;

	if (error != 0 || transferredBytes == 0) {
		EnterCriticalSection(&criticalSection);
		int index;

		for (index = 0; index < nClients; index++) {
			if (clients[index]->socket == sockInfo->socket)
				break;
		}

		closesocket(clients[index]->socket);
		delete clients[index]->pApp;
		GlobalFree(clients[index]);
		clients[index] = 0;
		for (int i = index; i < nClients - 1; i++)
			clients[i] = clients[i + 1];
		nClients--;
		LeaveCriticalSection(&criticalSection);

		return;
	}
	bool check = false;
	int key = 0;
	if (sockInfo->operation == RECEIVE) {
		sockInfo->recvBytes = transferredBytes;
		sockInfo->sentBytes = 0;
		sockInfo->operation = SEND;

		check = sockInfo->pApp->forwardData(sockInfo->dataBuf.buf, sockInfo->recvBytes);
		if (check == false) {
			freeSessionResource(sockInfo);
			return;
		}
	}

	check = sockInfo->pApp->processData();
	if (!check) {
		freeSessionResource(sockInfo);
		return;
	}
	key = sockInfo->pApp->getCtrlKey();
	switch (key) {
	case SEND_KEY:
	{
		ZeroMemory(&(sockInfo->overlapped), sizeof(WSAOVERLAPPED));
		
		check = sockInfo->pApp->retrieveData(sockInfo->buffer, &sockInfo->sentBytes);
		if (!check) {
			freeSessionResource(sockInfo);
			return;
		}
		sockInfo->dataBuf.buf = sockInfo->buffer;
		sockInfo->dataBuf.len = sockInfo->sentBytes;
		sockInfo->operation = SEND;

		if (WSASend(sockInfo->socket, &(sockInfo->dataBuf), 1, &sentBytes, 0, &(sockInfo->overlapped), workerRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				freeSessionResource(sockInfo);
				cout << "WSASend() failed with error " << WSAGetLastError() << endl;
				return;
			}
		}
		break;
	}
	case RECEIVE_KEY:
	{
		sockInfo->recvBytes = 0;
		flags = 0;
		ZeroMemory(&(sockInfo->overlapped), sizeof(WSAOVERLAPPED));
		sockInfo->dataBuf.len = DATA_BUFSIZE;
		sockInfo->dataBuf.buf = sockInfo->buffer;
		sockInfo->operation = RECEIVE;
		if (WSARecv(sockInfo->socket, &(sockInfo->dataBuf), 1, &recvBytes, &flags, &(sockInfo->overlapped), workerRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				freeSessionResource(sockInfo);
				cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
				return;
			}
		}
		break;
	}
	case SHUTDOWN_KEY:
	{
		freeSessionResource(sockInfo);
		return;
	}

	default:
	{
		break;
	}
	}
	return;
}

int main(int argc, char** argv)
{

	WSADATA wsaData;
	SOCKET listenSocket;
	SOCKADDR_IN serverAddr, clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	int ret;
	WSAEVENT acceptEvent;
	serverPort = (u_short)atoi(argv[1]);

	InitializeCriticalSection(&criticalSection);
	if ((ret = WSAStartup((2, 2), &wsaData) != 0)) {
		cout << "WSAStartup() failed with error " << ret << endl;
		WSACleanup();
		return -1;
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

	if (listen(listenSocket, 20)) {
		cout << "listen() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	cout << "Server started" << endl;
	if ((acceptEvent = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		cout << "WSACreateEvent() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	_beginthreadex(0, 0, ioThread, (LPVOID)acceptEvent, 0, 0);

	while (1) {
		if ((acceptSocket = accept(listenSocket, (PSOCKADDR)&clientAddr, &clientAddrLen)) == SOCKET_ERROR)
		{
			cout << "accept() failed with error " << WSAGetLastError() << endl;
			return 1;
		}

		if (WSASetEvent(acceptEvent) == FALSE)
		{
			cout << "WSASetEvent() failed with error " << WSAGetLastError() << endl;
			return 1;
		}
	}

	DeleteCriticalSection(&criticalSection);
	closesocket(listenSocket);
	WSACleanup();
	return 0;
}