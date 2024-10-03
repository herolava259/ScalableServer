#include<Winsock2.h>
#include<Windows.h>
#include<WS2tcpip.h>

#include<sstream>
#include<fstream>
#include<string>
#include<iostream>


#include<stdlib.h>
#include<time.h>

#include<stdio.h>

#pragma comment(lib,"Ws2_32.lib")

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

#define LIMIT_INTERGER 0xffffffff
#define LIMIT_TRY_NUM 10

#define RECEIVE 0
#define SEND 1


using namespace std;

typedef struct{
	
	char opcode;
	char key;
	unsigned int length;
	string fileName;

}InitedData;

typedef struct {
	char opcode;
	unsigned int length;
}DataHeader;

class Utils {
private:
	static Utils* pInstance;
	Utils() {
	}
public:
	Utils* getInstance(){
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

Utils* Utils::pInstance = nullptr;

typedef struct {
	SOCKET socket;
	WSAOVERLAPPED overlapped;
	WSABUF dataBuf;
	char buffer[DATA_BUFSIZE];
	int operation;
	int sentBytes;
	int recvBytes;
}SocketInfo;

class EncDecSession {
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
	SocketInfo* tcpLayer;

	void encryptData(char* dat, int length) {
		for (int i = 0; i < length; i++) {
			dat[i] = Utils::encode(dat[i], key, type);
		}
	}
public:
	EncDecSession(int idx, SocketInfo* tcpLayer) {
		index = idx;
		type = INIT_TYPE;
		status = NORMAL_STATUS;
		state = INIT_STATE;
		packageLength = 0;
		fileLength = 0;
		key = 0;
		offsetFile = 0;
		this->tcpLayer = tcpLayer;
	}

	int getState() {
		return state;
	}
	int getStatus() {
		return status;
	}
	bool startSession() {
		char* buff = tcpLayer->dataBuf.buf;
		InitedData pack = Utils::init_session_with_first_data(buff, index);
		if (pack.opcode == ENC_CODE) {
			type = ENC_TYPE;
			
		}
		else if (pack.opcode == DEC_CODE) {
			type = DEC_CODE;
		}
		else {
			status = SHUTDOWN_STATUS;
			return false;
		}
		state = WRITE_STATE;
		fileName = pack.fileName;
		key = pack.key;
		return true;
	}

	bool writeDataToFile() {
		cout << "Before WriteDataToFile: " << endl;
		if ((state != WRITE_STATE || state != PENDING_STATE) && tcpLayer->operation != RECEIVE) {
			status = SHUTDOWN_STATUS;
			return false;
		}

		cout << "In WriteDataToFile: " << endl;
		char* buff = tcpLayer->dataBuf.buf;
		unsigned int len = tcpLayer->recvBytes;
		if (state == PENDING_STATE) {
			int first = strlen(pendingBytes);
			for (int i = 0; i < OPCODE_SIZE + LENGTH_FIELD_SIZE - first; i++) {
				pendingBytes[first + i] = *buff;
				buff = buff + 1;
				len--;
			}
			pendingBytes[OPCODE_SIZE + LENGTH_FIELD_SIZE] = 0;
			DataHeader tmp = Utils::retrieveHeader(pendingBytes);
			if (tmp.opcode != TRANS_CODE) {
				status = SHUTDOWN_STATUS;
				return false;
			}
			packageLength = tmp.length;
			if (packageLength == 0) {
				state = READ_STATE;
				return true;
			}
			state = WRITE_STATE;
		}


		if (packageLength == 0) {
			cout << "In writeDataToFile: If packageLength == 0: " << status << endl;
			DataHeader header = Utils::retrieveHeader(buff);
			if (header.opcode != TRANS_CODE) {
				
				status = SHUTDOWN_STATUS;
				cout << "In writeDataToFile: In shutdown status: " << status << endl;
				return false;
			}
			cout << "In writeDataToFile: middle if packageLength == 0, status: " << status << "state: " << "state" << endl;
			packageLength = header.length;
			if (packageLength == 0) {
				state = READ_STATE;
				return true;
			}
			buff = buff + OPCODE_SIZE + LENGTH_FIELD_SIZE;
			len -= OPCODE_SIZE + LENGTH_FIELD_SIZE;
			cout << "In writeDataToFile: end if packageLength == 0, status: " << status << "state: " <<"state"<< endl;
		}

		fstream tmpStream;
		cout << "WriteDataToFile(): Before open" << endl;
		tmpStream.open(fileName, ios::out | ios::app | ios::binary);
		cout << "WriteDataToFile(): Before while" << endl;
		while (len > 0) {
			if (packageLength < len) {
				cout << "From WriteDataToFile: In if.while" << endl;
				len -= packageLength;
				if (!(Utils::write_data_to_file(tmpStream, buff, packageLength))) {
					status = SHUTDOWN_STATUS;
					tmpStream.close();
					return false;
				}
				else {
					fileLength += packageLength;
				}
				buff = buff+ packageLength;
				if (len < OPCODE_SIZE + LENGTH_FIELD_SIZE) {
					for (unsigned int i = 0; i < len; i++){
						pendingBytes[i] = buff[i];
					}
					pendingBytes[len] = 0;
					state = PENDING_STATE;
					tmpStream.close();
					return true;
				}
				DataHeader tmpHeader = Utils::retrieveHeader(buff);
				if (tmpHeader.opcode != TRANS_CODE) {
					tmpStream.close();
					status = SHUTDOWN_STATUS;
					return false;
				}
				packageLength = tmpHeader.length;
				if (packageLength == 0) {
					state = READ_STATE;
					tmpStream.close();
					return true;
				}
				buff = buff + OPCODE_SIZE + LENGTH_FIELD_SIZE;
				len -= OPCODE_SIZE + LENGTH_FIELD_SIZE;
			}
			else {
				
				if (!Utils::write_data_to_file(tmpStream, buff, len)) {
					status = SHUTDOWN_STATUS;
					tmpStream.close();
					return false;
				}
				buff[len] = 0;
				cout << "In last else.while of wirteDatatoFile() with write data to file: " <<status << ", " << buff << endl;
				cout << "State: " << state << endl;
				fileLength += len;
				packageLength -= len;
				break;
			}
		}
		tmpStream.close();
		return true;
	}

	bool readDataFromFileAndEncrypt() {
		cout << "In readDataFromFileAndEncrypt()" << endl;
		cout << "fileLength: " << fileLength << endl;
		if (state != READ_STATE || tcpLayer->operation != SEND) {
			cout << "in readDataFromFileAndEncrypt(): if check " << endl;
			status = SHUTDOWN_STATUS;
			return false;
		}
		unsigned int len = 0, offset =0;
		char* buff = tcpLayer->buffer;
		tcpLayer->dataBuf.buf = buff;
		cout << "In readDataFromFileAndEncrypt() : before if (packageLength == 0)" << endl;
		if (packageLength == 0) {
			packageLength = (fileLength - offsetFile) < LIMIT_INTERGER ? (fileLength - offsetFile) : LIMIT_INTERGER;
			buff[0] = TRANS_CODE;
			Utils::interger_to_byteArr(packageLength, buff + OPCODE_SIZE);
			offset = OPCODE_SIZE + LENGTH_FIELD_SIZE;
			
		}

		fstream tmpStream;
		tmpStream.open(fileName, ios::in | ios::binary);
		tmpStream.seekg((streamoff)offsetFile, ios::beg);
		cout << "In readDataFromFileAndEncrypt() : After open and seekg file" << endl;
		len = Utils::read_data_from_file(tmpStream, buff + offset, DATA_BUFSIZE - offset);
		if (len == -1) {
			cout << "In readDataFromFileAndEncrypt(): Error read file" << endl;
			status = SHUTDOWN_STATUS;
			tmpStream.close();
			return false;
		}
		encryptData(buff + offset, len);
		offsetFile += len;
		packageLength -= len;
		if (offsetFile >= fileLength) {
			cout << "In readDataFromFileAndEncrypt():Begin FINISH_STATE" << endl;
			state = FINISH_STATE;
		}
		tcpLayer->dataBuf.len = len + offset;
		tmpStream.close();
		return true;
	}

	bool writeFinish() {
		if (state != FINISH_STATE || status == SHUTDOWN_STATUS) {
			return false;
		}

		tcpLayer->dataBuf.buf = tcpLayer->buffer;
		char* buf = tcpLayer->buffer;
		buf[0] = TRANS_CODE;
		Utils::interger_to_byteArr(0, buf + OPCODE_SIZE);
		tcpLayer->dataBuf.len = OPCODE_SIZE + LENGTH_FIELD_SIZE;
		status = SHUTDOWN_STATUS;
		return true;
	}

	bool writeError() {
		if (status != SHUTDOWN_STATUS) {
			return false;
		}
		tcpLayer->dataBuf.buf = tcpLayer->buffer;
		char* buf = tcpLayer->buffer;
		buf[0] = ERROR_CODE;
		Utils::interger_to_byteArr(0, buf + OPCODE_SIZE);
		tcpLayer->dataBuf.len = OPCODE_SIZE + LENGTH_FIELD_SIZE;
		return true;
	}

	void finish() {
		remove(fileName.c_str());
	}
};

void freeSockInfo(SocketInfo* siArray[], int n) {
	closesocket(siArray[n]->socket);
	free(siArray[n]);
	for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
		siArray[i] = siArray[i + 1];
	}
}

void closeEventInArray(WSAEVENT eventArr[], int n) {
	WSACloseEvent(eventArr[n]);

	for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
		eventArr[i] = eventArr[i + 1];
	}
}

void freeResource(int idx, SocketInfo* infs[], EncDecSession* ppSess[], WSAEVENT eventArr[], int n) {
	closesocket(infs[idx]->socket);
	WSACloseEvent(eventArr[idx]);
	ppSess[idx]->finish();
	free(infs[idx]);
	delete ppSess[idx];
	for (int i = idx; i < n - 1; i++) {
		infs[i] = infs[i + 1];
		ppSess[i] = ppSess[i + 1];
		eventArr[i] = eventArr[i + 1];
	}
}

int main(int argc, char** argv)
{
	u_short serverPort = 0;
	SocketInfo* socks[WSA_MAXIMUM_WAIT_EVENTS];
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	EncDecSession* pSess[WSA_MAXIMUM_WAIT_EVENTS];

	serverPort = (u_short)atoi(argv[1]);

	DWORD nEvents = 0;

	WSADATA wsaData;
	if (WSAStartup((2, 2), &wsaData)) {
		cout << "WSAStartup() failed with error " << WSAGetLastError() << endl;
		return 1;
	}

	for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		socks[i] = 0;
		events[i] = 0;
		pSess[i] = nullptr;
	}

	SOCKET listenSock;
	if ((listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		cout << "Error" << WSAGetLastError() << ": Cannot create server socket. " << endl;
		return 1;
	}

	events[0] = WSACreateEvent();
	WSAEventSelect(listenSock, events[0], FD_ACCEPT | FD_CLOSE);
	nEvents++;

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error "<<WSAGetLastError() << " :Cannot bind this address " << endl;
		return 1;
	}
	if (listen(listenSock, 10)) {
		cout << "Error " << WSAGetLastError() << " :Cannot place socket to LISTEN STATE " << endl;
		return 1;
	}
	
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	SOCKET connSock;
	int index = 0;
	cout << "Server started " << endl;
	while (1) {
		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);

		if (index == WSA_WAIT_FAILED) {
			cout << "Error " << WSAGetLastError() << " :WaitForMultipleEvent() failed" << endl;
			break;
		}

		index = index - WSA_WAIT_EVENT_0;
		DWORD flags, transferredBytes;

		if (index == 0) {
			WSAResetEvent(events[0]);
			connSock = accept(listenSock, (PSOCKADDR)&clientAddr, &clientAddrLen);
			if (connSock == INVALID_SOCKET) {
				cout << "Error " << WSAGetLastError() << ": Cannot accept incomming connection " << endl;
				continue;
			}

			int i;
			if (nEvents >= WSA_MAXIMUM_WAIT_EVENTS) {
				cout << "Cannot permit coming connection " << endl;
				closesocket(connSock);
			}
			else {
				WSAEventSelect(connSock, NULL, 0);

				i = nEvents++;
				events[i] = WSACreateEvent();
				socks[i] = (SocketInfo*)malloc(sizeof(SocketInfo));
				socks[i]->socket = connSock;
				memset(&socks[i]->overlapped, 0, sizeof(WSAOVERLAPPED));
				socks[i]->overlapped.hEvent = events[i];
				socks[i]->dataBuf.buf = socks[i]->buffer;
				socks[i]->dataBuf.len = DATA_BUFSIZE;
				socks[i]->operation = RECEIVE;
				socks[i]->recvBytes = 0;
				socks[i]->sentBytes = 0;
				pSess[i] = new EncDecSession(i, socks[i]);

				flags = 0;
				if (WSARecv(socks[i]->socket, &(socks[i]->dataBuf), 1, &transferredBytes, &flags, &(socks[i]->overlapped), NULL) == SOCKET_ERROR) {
					//int errorCode = WSAGetLastError();
					if (WSAGetLastError() != WSA_IO_PENDING) {
						cout << "In (if index == 0), WSARecv failed with error " << WSAGetLastError() << endl;
						freeResource(i, socks, pSess, events, nEvents);
						nEvents--;
					}

					
				}

				cout << "Accept a client" << endl;
				
			}
		}
		else {
			SocketInfo* client;
			EncDecSession* session;
			client = socks[index];
			session = pSess[index];
			WSAResetEvent(events[index]);
			BOOL result;

			
			result = WSAGetOverlappedResult(client->socket, &(client->overlapped), &transferredBytes, FALSE, &flags);

			if (result == FALSE || transferredBytes == 0 || (session->getStatus() == SHUTDOWN_STATUS )) {
				freeResource(index, socks, pSess, events, nEvents);
				nEvents--;
				client = nullptr;
				session = nullptr;
				continue;
			}

			if (client->operation == RECEIVE) {
				client->recvBytes = transferredBytes;
				client->dataBuf.buf[client->recvBytes] = 0;
				client->sentBytes = 0;
				if (session->getState() == INIT_STATE) {
					cout << "in INIT_STATE" << endl;
					result = session->startSession();
					if (result == false && session->getStatus() ==SHUTDOWN_STATUS) {
						session->writeError();
						client->operation = SEND;
					}
					
				}
				else if(session->getState() == WRITE_STATE || session->getState() == PENDING_STATE) {
					result = session->writeDataToFile();
					if (result == false && session->getStatus() == SHUTDOWN_STATUS) {
						session->writeError();
						client->operation = SEND;
					}
					else if (session->getState() == READ_STATE) {
						client->operation = SEND;
						result = session->readDataFromFileAndEncrypt();
						if (result == false && session->getStatus() == SHUTDOWN_STATUS) {
							session->writeError();
						}
					}
					
				}
				/*else if (session->getState() == READ_STATE) {
					result = session->readDataFromFileAndEncrypt();
					if (result == false && session->getStatus() == SHUTDOWN_STATUS) {
						session->writeError();
					}
					client->operation = SEND;
				}*/
				else {
					session->writeError();
					client->operation = SEND;
				}
			}
			else if (client->operation == SEND) {
				if (session->getState() == READ_STATE) {
					result = session->readDataFromFileAndEncrypt();
					if (result == false && session->getStatus() == SHUTDOWN_STATUS) {
						session->writeError();
					}
				}
				else if (session->getState() == FINISH_STATE) {
					result = session->writeFinish();
					if (result == false && session->getStatus() == SHUTDOWN_STATUS) {
						session->writeError();
					}
					cout << "In main: end Session is finish" << endl;
				}
				else {
					session->writeError();
				}
			}
			else {
				cout << "Error if 3 main " << endl;

			}

			int error = 0;
			if (client->operation == RECEIVE) {
				flags = 0;
				client->dataBuf.buf = client->buffer;
				client->dataBuf.len = DATA_BUFSIZE;
				client->recvBytes = 0;
				memset(&(client->overlapped), 0, sizeof(WSAOVERLAPPED));
				client->overlapped.hEvent = events[index];
				if (WSARecv(client->socket, &(client->dataBuf), 1, &transferredBytes, &flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
					error = WSAGetLastError();
				}
			}
			else {
				if (WSASend(client->socket, &(client->dataBuf), 1, &transferredBytes, flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
					error = WSAGetLastError();
				}
			}

			if (error != WSA_IO_PENDING && error != 0) {
				cout << "I/O in socket failed with error " << error << endl;
				freeResource(index, socks, pSess, events, nEvents);
				nEvents--;
				client = 0;
				session = 0;
			}
		}
	}
	closesocket(listenSock);
	WSACleanup();
	return 0;
}