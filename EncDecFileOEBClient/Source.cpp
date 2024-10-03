#include<Winsock2.h>
#include<Windows.h>
#include<WS2tcpip.h>

#include<iostream>
#include<fstream>
#include<string>
#include<sstream>
#include<regex>
#include<stack>
#include<map>
#include<queue>

#include<stdlib.h>
#include<time.h>
#include<stdio.h>
#include<string.h>
#include<cstdio>

#define DATA_BUFSIZE 8192
#define OPCODE_SIZE 1
#define LENGTH_FIELD_SIZE 4
#define LIMIT_PACK_SIZE 0xffffffff

#define ENC_CODE 0
#define DEC_CODE 1
#define TRANS_CODE 2
#define ERROR_CODE 3

#define INIT_STATE 0
#define ERROR_STATE 1

#define START 4
#define RECEIVE 2
#define SEND 3

#define WWAIT_TIMEOUT 0
#define WWAIT_FINISHED 1
#define WWAIT_FAILED 2

#define ENC_TYPE 0
#define DEC_TYPE 1

#define LENGTH_NAME 7
#define ASSESTS_DIR "C:\\Users\\Admin\\Source\\Repos\\EncDecFileOEBClient\\Debug\\assests\\"

// hang so dieu khien cli tu controller
#define CLI_DATA 1<<0
#define CLI_LOG 1<<1
#define CLI_EXIT 1<<2
#define CLI_WRONG_CMD 1<<3
#define CLI_ALL_LOG 1<<4

#define WRONG_MESS "you entered the wrong syntax"

#define MAX_CHILD 10

#define COMMAND_PATTERN "\\s+.*$"

#define EXIT_IDX 0
#define ENC_IDX 1
#define DEC_IDX 2

#define KEY_IDX 0
#define LEAF_IDX 1

#define DEPTH_TREE 4
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

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
	/*static bool exists_file(const std::string& name) {
		FILE* file;
		errno_t err;
		bool check;
		if ((err= fopen_s(&file, name.c_str(), "r")) != 0) {
			
			check = true;
		}
		else {
			check = false;
		}
		fclose(file);
		return check;
	}*/

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

	static bool write_data_to_file(fstream& stream, char* dat, int max_length) {

		for (int i = 0; i < max_length; i++) {
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
	static unsigned long long getFileSize(string fileName) {
		unsigned long long len = 0;
		ifstream is(fileName, ifstream::binary);
		if (is) {
			is.seekg(0, is.end);
			len = is.tellg();
			is.close();
		}

		return len;
	}
};
Utils* Utils::pInstance = nullptr;

class EncDecLogger {
private:
	static EncDecLogger* pInstance;
	static stack<string> log_stack;
	EncDecLogger() {}
public:
	
	static EncDecLogger* getInstance() {
		if (pInstance == nullptr) {
			pInstance = new EncDecLogger();
		}
		return pInstance;
	}

	static void pushLogError(int error, string message) {
		stringstream ss;
		ss << "TCP Error " << error << ": " << message;
		log_stack.push(ss.str());
	}

	static void pushMessage(string mess) {
		log_stack.push(mess);
	}

	static bool isEmpty() {
		return log_stack.empty();
	}
	static string getLastLog() {
		
		string log = "";
		if (!log_stack.empty()) {
			log = log_stack.top();
			log_stack.pop();
		}
		return log;
	}
	~EncDecLogger() {
		delete pInstance;
	}
};
EncDecLogger* EncDecLogger::pInstance = nullptr;
stack<string> EncDecLogger::log_stack;

typedef struct {
	SOCKET socket;
	WSAOVERLAPPED overlapped;
	WSABUF dataBuf;
	char buffer[DATA_BUFSIZE];
	int operation;
	int sentBytes;
	int recvBytes;
}SocketInfo;


class OverlappedConnection {
private:
	SocketInfo* pSockInf;
	WSAEVENT events[1];
	int state;
	int error;
	bool isIO;
	char serverIP[INET_ADDRSTRLEN];
	u_short serverPort;
	DWORD transferredBytes, flags;
public:
	int getState() {
		return state;
	}
	int getError() {
		return error;
	}
	int getCurrOperation() {
		return pSockInf->operation;
	}
	OverlappedConnection() {
		memset(serverIP, 0, INET_ADDRSTRLEN);
		serverPort = 0;
		error = 0;
		transferredBytes = 0;
		flags = 0;
		isIO = false;
		pSockInf = (SocketInfo*)malloc(sizeof(SocketInfo));
		if (pSockInf == NULL) {
			state = ERROR_STATE;
			return;
		}
	}

	bool setServerAddress(char serverIP[], u_short serverPort) {

		strcpy_s(this->serverIP, INET_ADDRSTRLEN, serverIP);
		this->serverPort = serverPort;
		if (state == ERROR_STATE) {
			return false;
		}
		return true;
	}

	bool reconnect() {
		
		error = 0;
		isIO = false;
		state = INIT_STATE;
		transferredBytes = 0;
		flags = 0;
		pSockInf->dataBuf.buf = pSockInf->buffer;
		pSockInf->dataBuf.len = DATA_BUFSIZE;
		pSockInf->operation = START;
		memset(&(pSockInf->overlapped), 0, sizeof(WSAOVERLAPPED));
		*events = WSACreateEvent();
		if (*events == WSA_INVALID_EVENT) {
			state = ERROR_STATE;
			error = WSAGetLastError();
			return false;
		}
		pSockInf->overlapped.hEvent = *events;

		pSockInf->socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (pSockInf->socket == INVALID_SOCKET) {
			state = ERROR_STATE;
			error = WSAGetLastError();
			return false;
		}
		int tv = 100000;
		setsockopt(pSockInf->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));
		
		sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(serverPort);
		inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
		if (WSAConnect(pSockInf->socket, (sockaddr*)&serverAddr, sizeof(serverAddr), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
			if ((error = WSAGetLastError()) != WSAEWOULDBLOCK) {
				state = ERROR_STATE;
				close();
				return false;
			}
		}
		return true;
	}

	bool freeResource() {
		WSACloseEvent(*events);
		closesocket(pSockInf->socket);
		free(pSockInf);
		return true;
	}

	bool pendingReceive() {
		if (state == ERROR_STATE || isIO) {
			return false;
		}

		flags = 0;
		pSockInf->operation = RECEIVE;
		pSockInf->dataBuf.buf = pSockInf->buffer;
		pSockInf->dataBuf.len = DATA_BUFSIZE;
		memset(&(pSockInf->overlapped), 0, sizeof(WSAOVERLAPPED));
		pSockInf->overlapped.hEvent = *events;
		if (WSARecv(pSockInf->socket, &(pSockInf->dataBuf), 1, &transferredBytes, &flags, &(pSockInf->overlapped), NULL) == SOCKET_ERROR) {
			error = WSAGetLastError();
			if (error != WSA_IO_PENDING) {
				state = ERROR_STATE;
				return false;
			}
		}
		isIO = true;
		pSockInf->recvBytes = 0;
		pSockInf->sentBytes = 0;
		return true;
	}

	bool pendingSend(char* inData, int length) {

		if (state == ERROR_STATE || isIO) {
			return false;
		}

		int n = length > DATA_BUFSIZE ? DATA_BUFSIZE : length;
		for (int i = 0; i < n; i++) {
			pSockInf->buffer[i] = inData[i];
		}

		pSockInf->operation = SEND;
		pSockInf->dataBuf.buf = pSockInf->buffer;
		pSockInf->dataBuf.len = n;
		if (WSASend(pSockInf->socket, &(pSockInf->dataBuf), 1, &transferredBytes, flags, &(pSockInf->overlapped), NULL) == SOCKET_ERROR) {
			error = WSAGetLastError();
			if (error != WSA_IO_PENDING) {
				state = ERROR_STATE;
				return false;
			}
		}
		isIO = true;
		pSockInf->sentBytes = n;
		pSockInf->recvBytes = 0;
		return true;
	}

	int waitForCompletedIO(DWORD intervalTimeout = WSA_INFINITE) {
		int index;
		if (state == ERROR_STATE || !isIO) {
			return WWAIT_FAILED;
		}
		index = WSAWaitForMultipleEvents(1, events, FALSE, intervalTimeout, FALSE);
		if (index == WSA_WAIT_TIMEOUT) {
			return WWAIT_TIMEOUT;
		}

		if (index == WSA_WAIT_FAILED) {
			error = WSAGetLastError();
			state = ERROR_STATE;
			return WWAIT_FAILED;
		}
		WSAResetEvent(*events);
		BOOL result;
		result = WSAGetOverlappedResult(pSockInf->socket, &(pSockInf->overlapped), &transferredBytes, FALSE, &flags);
		if (result == FALSE || transferredBytes == 0) {
			error = WSAGetLastError();
			state = ERROR_STATE;
			return WWAIT_FAILED;
		}
		if (pSockInf->operation == RECEIVE) {
			pSockInf->recvBytes = transferredBytes;
		}
		isIO = false;

		return WWAIT_FINISHED;
	}

	bool getReceivedData(char* outBuff, int* length) {
		if (state == ERROR_STATE || (pSockInf->operation != RECEIVE) || isIO) {
			return false;
		}
		if (pSockInf->recvBytes == 0) {
			return false;
		}
		*length = pSockInf->recvBytes;
		for (int i = 0; i < *length; i++) {
			*(outBuff + i) = pSockInf->dataBuf.buf[i];
		}
		return true;
	}

	void refresh() {
		WSACloseEvent(*events);
		*events = WSACreateEvent();
		if (*events == WSA_INVALID_EVENT) {
			state = ERROR_STATE;
			error = WSAGetLastError();
			return;
		}
		error = 0;
		isIO = false;
		state = INIT_STATE;
		transferredBytes = 0;
		flags = 0;
		pSockInf->dataBuf.buf = pSockInf->buffer;
		pSockInf->dataBuf.len = DATA_BUFSIZE;
		pSockInf->operation = START;
		memset(&(pSockInf->overlapped), 0, sizeof(WSAOVERLAPPED));
		pSockInf->overlapped.hEvent = *events;
	}

	void close() {
		closesocket(pSockInf->socket);
		WSACloseEvent(*events);
	}
};

class EncDecClient {
private:
	string fileName;
	string dstFileName;
	OverlappedConnection* pConn;
	int encryptType;
	char key;
	char buff[DATA_BUFSIZE];
public:
	EncDecClient(OverlappedConnection* pConn = NULL) {
		this->pConn = NULL;
		fileName = "abc.txt";
		dstFileName = "abc.txt.enc";
		encryptType = 0;
		key = 0;
	}
	void setConnection(OverlappedConnection* pConn) {
		this->pConn = pConn;
	}
	void setFileName(string fileName) {
		this->fileName = fileName;
	}

	void create_dst_fileName() {
		if (encryptType == ENC_TYPE) {
			size_t len = fileName.size();
			dstFileName = fileName.substr(0, len - 4) + ".enc";
		}
		else if (encryptType == DEC_TYPE) {
			dstFileName = fileName + ".dec";
		}
	}

	bool setEncryptTypeAndKey(int type, char key) {
		if (type > 1) {
			return false;
		}
		this->key = key;
		this->encryptType = type;
		
		return true;
	}

	void softRefresh() {
		remove(fileName.c_str());
		encryptType = -1;
	}

	bool hardRefresh() {
		softRefresh();
		pConn->freeResource();
		pConn = NULL;
	}

	bool requestEncDecFile() {
		int res;
		int len = OPCODE_SIZE + LENGTH_FIELD_SIZE;
		memset(buff, 0, DATA_BUFSIZE);
		if (encryptType == ENC_TYPE) {
			buff[0] = ENC_CODE;
		}
		else {
			buff[0] = DEC_CODE;
		}
		Utils::interger_to_byteArr(1, buff + OPCODE_SIZE);
		buff[OPCODE_SIZE + LENGTH_FIELD_SIZE] = key;
		len += 1;
		res = pConn->pendingSend(buff, len);
		if (res == 0) {
			EncDecLogger::pushLogError(pConn->getError(), "Cannot request server to enc-dec file");
			return false;
		}
		res = pConn->waitForCompletedIO();
		if (res == WWAIT_FAILED) {
			EncDecLogger::pushLogError(pConn->getError(), "Cannot wait to completed IO overlapped");
			return false;
		}
		return true;
	}

	bool sendFile() {
		unsigned long long fileLength = Utils::getFileSize(fileName);
		unsigned long long sentBytesSize = 0;
		int res = 0;
		if (fileLength <= 0) {
			EncDecLogger::pushMessage("Error I-O file: Cannot open file or file is empty with name " + fileName);
			return false;
		}
		fstream tmpStream;
		tmpStream.open(fileName, ios::in | ios::binary);
		tmpStream.seekg(0, tmpStream.beg);
		cout << "In EncDecClient.sendFile(): before loop 1" << endl;
		while (sentBytesSize < fileLength) {
			unsigned int len = 0;
			int offset = 0;
			if (fileLength - sentBytesSize > LIMIT_PACK_SIZE) {
				len = LIMIT_PACK_SIZE;
			}
			else {
				len = (unsigned int)(fileLength-sentBytesSize);
			}
			buff[0] = TRANS_CODE;
			Utils::interger_to_byteArr(len, buff + OPCODE_SIZE);
			sentBytesSize += len;
			offset = Utils::read_data_from_file(tmpStream, buff + OPCODE_SIZE + LENGTH_FIELD_SIZE, DATA_BUFSIZE - (OPCODE_SIZE + LENGTH_FIELD_SIZE));
			if (offset <= 0) {
				EncDecLogger::pushMessage("Error I-O file: Cannot read file again with name " + fileName);
				tmpStream.close();
				return false;
			}
			else {
				offset += OPCODE_SIZE + LENGTH_FIELD_SIZE;
			}

			cout << "In EncDecClient.sendFile(): before loop 2 in loop 1" << endl;
			while (len > 0) {

				cout << "In EncDecClient.sendFile(): before pendingSend" << endl;
				res = pConn->pendingSend(buff, offset);
				if (res == 0) {
					EncDecLogger::pushLogError(pConn->getError(), "Cannot send data file to server with overlapped I/O");
					tmpStream.close();
					return false;
				}
				len -= offset;
				offset = Utils::read_data_from_file(tmpStream, buff, DATA_BUFSIZE);
				if (offset <= 0) {
					len = 0;
				}
				cout << "In EncDecClient.sendFile(): before waitForCompletedI0" << endl;
				res = pConn->waitForCompletedIO();
				if (res == WWAIT_FAILED) {
					EncDecLogger::pushLogError(pConn->getError(), "Cannot wait to overlapped I/O operation is completed ");
					tmpStream.close();
					return false;
				}
			}
		}
		tmpStream.close();

		buff[0] = TRANS_CODE;
		Utils::interger_to_byteArr(0, buff + OPCODE_SIZE);

		cout << "In EncDecClient.sendFile(): before pendingSend" << endl;
		res = pConn->pendingSend(buff,OPCODE_SIZE+LENGTH_FIELD_SIZE);
		if (res == 0) {
			EncDecLogger::pushLogError(pConn->getError(), "Cannot send data file to server with overlapped I/O");
			return false;
		}
		cout << "In EncDecClient.sendFile(): before waitForCompletedI0" << endl;
		res = pConn->waitForCompletedIO();
		if (res == WWAIT_FAILED) {
			EncDecLogger::pushLogError(pConn->getError(), "Cannot wait to overlapped I/O operation is completed ");
			return false;
		}

		return true;
	}

	bool recvEncDecFile() {
		fstream tmpStream;
		int length = 0;
		int res = 0;
		unsigned int lenPack = 0;
		cout << "In EncDecClient.recvEncDecFile: before start" << endl;
		if (pConn->getState() == ERROR_STATE) {
			EncDecLogger::pushLogError(pConn->getError(), "OverlappedConnection have a error.");

			return false;
		}
		pConn->refresh();

		tmpStream.open(dstFileName, ios::out | ios::binary | ios::trunc);
		tmpStream.seekg(0, tmpStream.beg);
		cout << "In EncDecClient.recvEncDecFile: before loop" << endl;
		while (1) {
			cout << "In EncDecClient.recvEncDecFile: before pendingReceive" << endl;
			res = pConn->pendingReceive();
			if (res == false || pConn->getState() == ERROR_STATE) {
				EncDecLogger::pushLogError(pConn->getError(), "Cannot receive data");
				tmpStream.close();
				return false;
			}
			cout << "In EncDecClient.recvEncDecFile: before pConn->waitForCompletedIO()" << endl;
			res = pConn->waitForCompletedIO();
			if (res == WWAIT_FAILED || pConn->getState() == ERROR_STATE) {
				EncDecLogger::pushLogError(pConn->getError(), "waitForComplected IO failed");
				tmpStream.close();
				return false;
			}
			cout << "In EncDecClient.recvEncDecFile: before pConn->getReceivedData(buff, &length)" << endl;
			pConn->getReceivedData(buff, &length);

			if (buff[0] != TRANS_CODE || length < OPCODE_SIZE+LENGTH_FIELD_SIZE) {
				if (buff[0] == ERROR_CODE) {
					EncDecLogger::pushMessage("Server: Error from server while serving");
				}

				EncDecLogger::pushMessage("Error from Enc-Dec protocol");

				tmpStream.close();
				return false;
			}

			lenPack = Utils::byteArr_to_interger(buff + OPCODE_SIZE);
			if (lenPack == 0) {
				EncDecLogger::pushMessage("Shut down transmission file");
				break;
			}
			res = Utils::write_data_to_file(tmpStream, buff+OPCODE_SIZE+LENGTH_FIELD_SIZE, length-(OPCODE_SIZE+LENGTH_FIELD_SIZE));
			if (res == 0) {
				EncDecLogger::pushMessage("I/O file failed");
				tmpStream.close();
				return false;
			}

			lenPack -= length -(OPCODE_SIZE + LENGTH_FIELD_SIZE);
			cout << "In EncDecClient.recvEncDecFile: before loop2 in loop1" << endl;
			while (lenPack > 0) {
				cout << "In EncDecClient.recvEncDecFile: before pendingReceive" << endl;
				res = pConn->pendingReceive();
				if (res == false || pConn->getState() == ERROR_STATE) {
					EncDecLogger::pushLogError(pConn->getError(), "Cannot receive data");
					tmpStream.close();
					return false;
				}

				cout << "In EncDecClient.recvEncDecFile: before pConn->waitForCompletedIO()" << endl;
				res = pConn->waitForCompletedIO();
				if (res == WWAIT_FAILED || pConn->getState() == ERROR_STATE) {
					EncDecLogger::pushLogError(pConn->getError(), "waitForComplected IO failed");
					tmpStream.close();
					return false;
				}
				cout << "In EncDecClient.recvEncDecFile: before pConn->getReceivedData(buff, &length)" << endl;
				pConn->getReceivedData(buff, &length);
				res = Utils::write_data_to_file(tmpStream, buff, length);
				if (res == 0) {
					EncDecLogger::pushMessage("I/O file failed");
					tmpStream.close();
					return false;
				}
				lenPack -= length;
			}
		}
		tmpStream.close();
		return true;
	}

	bool reconnect() {
		if (pConn->reconnect()) {
			return true;
		}
		else {
			EncDecLogger::pushLogError(pConn->getError(), "Cannot reconnect to server");
		}
	}
	void closeConnect() {
		pConn->close();
	}

	bool isConnect() {
		if (pConn->getState() == ERROR_STATE) {
			EncDecLogger::pushMessage("Error from isConnect EncDecClient: Cannot connect to server");
			return false;
		}
		else {
			return true;
		}
	}
};

struct ResultData {
	int flag;
	string displayData;

	ResultData operator=(ResultData x){
		flag = x.flag;
		displayData = x.displayData;
		return x;
	};

};

struct TraversalData {
	int flags[DEPTH_TREE];
	int depth;
	map<string, string> extData;
};
struct ResolveLeaf {
	string command;
	string argument;
};
struct ResolveNode {
	string command;
	int nChild;
	int nLeaf;
	ResolveNode** childNodes;
	ResolveLeaf** leafNodes;
};

class BaseResolver {
protected:
	
public:
	BaseResolver() {}
};
class CommandResolver: public BaseResolver{
private:
	ResolveNode* pRoot;
	void createResolveTree() {
		ResolveNode* pTmpNode;
		ResolveLeaf* pTmpLeaf;

		pRoot->command = "encryptFile";
		pRoot->nChild = 3;
		pRoot->nLeaf= 0;
		pRoot->childNodes = new ResolveNode * [3];

		pTmpNode = new ResolveNode;
		pTmpNode->command = "exit";
		pTmpNode->nChild = 0;
		pTmpNode->nLeaf = 0;
		pTmpNode->childNodes = NULL;
		pTmpNode->leafNodes = NULL;
		pRoot->childNodes[0] = pTmpNode;

		pTmpNode = new ResolveNode;
		pTmpNode->command = "encode";
		pTmpNode->nChild = 0;
		pTmpNode->nLeaf = 2;
		pTmpNode->childNodes = NULL;
		pTmpNode->leafNodes = new ResolveLeaf * [2];
		pRoot->childNodes[1] = pTmpNode;

		pTmpLeaf = new ResolveLeaf;
		pTmpLeaf->command = "-f";
		pTmpLeaf->argument = "^(?!.*_).*\\.(txt|png|jpeg)(\\.enc)?$";
		pTmpNode->leafNodes[0] = pTmpLeaf;

		pTmpLeaf = new ResolveLeaf;
		pTmpLeaf->command = "-k";
		pTmpLeaf->argument = "^(25[0-5]|((2[0-4])|(1\\d)|([1-9]))?\\d)$";
		pTmpNode->leafNodes[1] = pTmpLeaf;

		pTmpNode = new ResolveNode;
		pTmpNode->command = "decode";
		pTmpNode->nChild = 0;
		pTmpNode->nLeaf = 2;
		pTmpNode->childNodes = NULL;
		pTmpNode->leafNodes = new ResolveLeaf * [2];
		pRoot->childNodes[2] = pTmpNode;

		pTmpLeaf = new ResolveLeaf;
		pTmpLeaf->command = "-f";
		pTmpLeaf->argument = "^(?!.*_).*\\.(txt|png|jpeg)(\\.dec)?$";
		pTmpNode->leafNodes[0] = pTmpLeaf;

		pTmpLeaf = new ResolveLeaf;
		pTmpLeaf->command = "-k";
		pTmpLeaf->argument = "^(25[0-5]|((2[0-4])|(1\\d)|([1-9]))?\\d)$";
		pTmpNode->leafNodes[1] = pTmpLeaf;
	}

	bool treeTraversal(queue<string>& q, TraversalData& dat, ResolveNode* pNode, int idx) {

		cout << "in treeTraversal " << idx << endl;
		dat.depth = idx + 1;
		bool check = true;
		string command = "";
		if (!q.empty()) {
			command = q.front();
		}
		else {
			if (pNode->nChild == 0 && pNode->nLeaf == 0) {
				return true;
			}
			else {
				dat.extData["error"] = "Error travesal node";
				return false;
			}
		}
		for (int i = 0; i < pNode->nChild; i++) {
			if (!command.compare(pNode->childNodes[i]->command)) {
				q.pop();
				dat.flags[idx + 1] = i;
				return treeTraversal(q, dat, pNode->childNodes[i], idx + 1);
			}
		}

		check = parseSeqToLeafsOfNode(q, dat, pNode, idx);

		if (!q.empty()) {
			dat.extData["error"] += " and Queue in end time is not empty";
			return false;
		}
		return check;
	}

	bool parseSeqToLeafsOfNode(queue<string>& q, TraversalData& dat, ResolveNode* pNode, int idx) {

		bool check = true;
		string tmp;
		while (check && !q.empty()) {
			check = false;
			tmp = q.front();
			for (int i = 0; i < pNode->nLeaf && !check; i++) {
				if (!tmp.compare(pNode->leafNodes[i]->command)) {
					q.pop();
					dat.flags[idx + 1] = i;
					if (!extractLeaf(q, dat, pNode->leafNodes[i], idx + 1)) {
						return false;
					}
					check = true;
				}
			}
		}
		if (dat.flags[idx + 1] == -1) {
			dat.extData["error"] = " Wrong CMD at parseSeqToLeafsOfNode";
			return false;
		}
		return true;
	}

	bool extractLeaf(queue<string>& q, TraversalData& dat, ResolveLeaf* pLeaf, int idx) {
		dat.depth = idx + 1;
		if (!q.empty()) {
			string sequence = q.front();
			q.pop();
			if (regex_match(sequence, regex(pLeaf->argument))) {
				dat.extData[pLeaf->command] = sequence;
				return true;
			}
			else {
				dat.extData["error"] = "Invalid argument";
				return false;
			}
		}
		else {
			dat.extData["error"] = "Not enough argument";
			return false;
		}
	}
public:
	CommandResolver():BaseResolver(){
		pRoot = new ResolveNode;
		createResolveTree();
	}
	bool extractTree(string sequence, TraversalData& dat) {
		dat.flags[0] = 0;
		dat.flags[1] = -1;
		dat.flags[2] = -1;
		dat.depth = 0;
		queue<string> q;
		regex pattern = regex("\\S+");
		smatch matches;
		
		while (regex_search(sequence, matches, pattern)){
			for (auto i : matches) {
				q.push(i.str());
			}
			
			sequence = matches.suffix().str();
		}
		
		if (!q.empty()) {
			string tmp = q.front();
			q.pop();
			
			if (!pRoot->command.compare(tmp)) {
				return  treeTraversal(q, dat, pRoot, 0);
			}
			else {
				dat.extData["error"] = "Wrong first command word";
				return false;
			}
		}

		dat.extData["error"] = "Empty sequence";
		return false;
		
	}
	void destroyNode(ResolveNode* pNode) {
		for (int i = 0; i < pNode->nChild; i++) {
			destroyNode(pNode->childNodes[i]);
		}
		delete pNode->childNodes;
		for (int i = 0; i < pNode->nLeaf; i++) {
			delete pNode->leafNodes[i];
		}
		delete pNode->leafNodes;
		delete pNode;
		return;
	}
	void freeMemory() {
		destroyNode(pRoot);
	}
	~CommandResolver() {
		destroyNode(pRoot);
	}
};

class EncDecFileController {
private:
	CommandResolver* pResolver;
	EncDecClient* pClient;
	bool checkExistFieldWithKey(map<string, string>& dict, string key) {
		map<string, string>::iterator it;
		it = dict.find(key);
		if (it == dict.end()) {
			return false;
		}
		else {
			return true;
		}
	}

	bool checkExistFile(string fileName) {
		ifstream inFile(fileName);
		return inFile.good();
	}
	bool processIOFile() {
		bool res = false;
		res = pClient->requestEncDecFile();
		if (res == false) {
			return false;
		}
		res = pClient->sendFile();
		if (!res) {
			return false;
		}
		return pClient->recvEncDecFile();
	}

	ResultData extendCMD(TraversalData& trvData) {
		ResultData res = { 0, "" };
		res.flag |= CLI_LOG | CLI_DATA;
		EncDecLogger::pushMessage("Error from Resolver");
		EncDecLogger::pushMessage(trvData.extData["error"]);
		res.displayData = "error Data";
		return { 0, "" };
	}

	ResultData processEncDecFile(int type, TraversalData& trvData, string name = "encode") {

		char key;
		string fileName = "";
		fileName += ASSESTS_DIR;
		ResultData res = { 0 , "" };
		if (checkExistFieldWithKey(trvData.extData, "-f") && checkExistFieldWithKey(trvData.extData, "-k")) {
			fileName += trvData.extData["-f"];
			if (pClient->reconnect() && checkExistFile(fileName)) {
				pClient->softRefresh();
				key = (char)atoi(trvData.extData["k"].c_str());
				pClient->setEncryptTypeAndKey(type, key);
				pClient->setFileName(fileName);
				pClient->create_dst_fileName();
				if (processIOFile()) {
					res.flag |= CLI_DATA;
					res.displayData += name+"file" + trvData.extData["-f"] + "is succesful";
				}
				else {

					res.flag |= CLI_ALL_LOG | CLI_EXIT;
					EncDecLogger::pushMessage("Cannot "+name+ "file from EncDecFileController.processInputData()");
				}

				pClient->closeConnect();
			}
			else {

				EncDecLogger::pushMessage("Error from check pClient->isConnect() and checkExistFile()");
				if (!pClient->isConnect()) {
					EncDecLogger::pushMessage("In processEncDecFile of controller: Cannot connect to server");
				}
				res.flag |= CLI_ALL_LOG;
			}

			
		}
		else {
			res.displayData += "Error from controller: not enough argument";
		}

		return res;
	}
public:
	EncDecFileController(EncDecClient* pClient) {
		pResolver = new CommandResolver();
		this->pClient = pClient;
	}

	ResultData start() {
		ResultData res = {0,""};
		/*cout << res.flag << endl;*/
		return res;
	}
	ResultData processInputData(string inpData){
		ResultData res = {0, ""};
		TraversalData trvData;
		bool check = false;
		char key = 0;
		trvData.extData["error"] = "No error";
		if (pResolver->extractTree(inpData, trvData)) {
			switch (trvData.flags[1]) {
			case EXIT_IDX:
			{
				cout << "in EXIT_IDX" << endl;
				res.flag |= CLI_EXIT | CLI_LOG;
				break;
			}
			case ENC_IDX:
			{
				/*char key = (char)atoi(trvData.extData["-k"].c_str());*/
				res = processEncDecFile(ENC_TYPE, trvData, "encode");
				break;
				//pClient->setEncryptTypeAndKey(ENC_TYPE, key);
// Code tiep phan ma hoa file

			}
			case DEC_IDX:
			{
				
				/*char key = (char)atoi(trvData.extData["-k"].c_str());*/
				res = processEncDecFile(DEC_TYPE, trvData, "decode");
				break;
			}
			default:
			{
				res = extendCMD(trvData);
				break;

			}

			}
		
		}
		else {
			res.displayData = "Ohh You entered wrong command.";
			res.displayData += " Error from resolver: " + trvData.extData["error"];
			res.flag |= CLI_DATA | CLI_ALL_LOG | CLI_WRONG_CMD;
		}
		return res;
	}
	~EncDecFileController() {
		delete pResolver;
	}

	ResultData end() {
		ResultData res = { 0,"" };
		pResolver->freeMemory();
		return res;
	}
};

/*
* chay vong loop tuong tac voi nguoi dung
* Nhan cau lenh tu nguoi dung
* truyen cau lenh den bo phan giai cau lenh 
* nhan lai ket qua cau lenh
* ket qua bao gom 
* + dieu khien ket thuc
* + du lieu hien thi 
* + cac dieu khien khac 
*/
class UserCLI {
private:
	EncDecFileController* pController;
	inline bool isExit(int flag) {
		if ((flag & CLI_EXIT) == CLI_EXIT) {
			return true;
		}
		else {
			return false;
		}
	}

	void _exit() {
		cout << "_exit: bye bye" << endl;
	}
public:
	UserCLI(EncDecFileController* pController) {
		this->pController = pController;
	}

	void displayGuide() {
		cout << "Welcome to Enc-Dec File Service" << endl;
	}
	void displayHelp() {

	}
	void displayStart() {
		cout << "This is displayStart()" << endl;

	}

	void displayEnd() {
		cout << "This is displayEnd() " << endl;
	}

	void loop() {
		ResultData result;
		stringstream ss;
		string tmp;
		ss.str("");
		cout << "Begin loop CLI" << endl;
		result = pController->start();
		cout << result.flag << endl;

		if ((result.flag & CLI_DATA) == CLI_DATA) {
			ss << result.displayData << "\n";
		}

		if ((result.flag & CLI_ALL_LOG) == CLI_ALL_LOG) {
			
			while (!EncDecLogger::isEmpty()) {
				tmp = EncDecLogger::getLastLog();
				ss << tmp << "\n";
			}
		}
		else if ((result.flag & CLI_LOG) == CLI_LOG) {
			if (!EncDecLogger::isEmpty()) {
				ss << EncDecLogger::getLastLog() << "\n";
			}
		}
		
		cout << ss.str() << endl;
		//cout << result.flag << endl;
		if (isExit(result.flag)) {
			cout << "in exit" << endl;
			_exit();
			return;
		}
		
		cout << "Before loop while CLI" << endl;
		while (1) {
			cout << ">>> ";
			getline(cin,tmp);
			cout << "process command..." << endl;
			result = pController->processInputData(tmp);
			
			ss.str("Begin:\n");

			if ((result.flag & CLI_WRONG_CMD) == CLI_WRONG_CMD) {
				ss << WRONG_MESS << "\n";
			}

			if ((result.flag & CLI_DATA) == CLI_DATA) {
				ss << result.displayData << "\n";
			}
			
			if ((result.flag & CLI_ALL_LOG) == CLI_ALL_LOG) {
				
				while (!EncDecLogger::isEmpty()) {
					tmp = EncDecLogger::getLastLog();
					ss << tmp << "\n";
				}
			}
			else if ((result.flag & CLI_LOG) == CLI_LOG) {
				
				if (!EncDecLogger::isEmpty()) {
					tmp = EncDecLogger::getLastLog();
					ss << tmp << "\n";
				}
			}

			
			cout << ss.str() << endl;
			if (isExit(result.flag)) {
				_exit();
				break;
			}

		}
		pController->end();
	}
};

int main(int argc, char** argv) {

	char serverIP[INET_ADDRSTRLEN];
	
	u_short serverPort = 0;

	string patternIP = "^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)(\\.(?!$)|$)){4}$";
	string patternPort = "^(\\d+){1,5}$";

	if (regex_match(argv[1], regex(patternIP))&&regex_match(argv[2], regex(patternPort))) {
		strcpy_s(serverIP, INET_ADDRSTRLEN, argv[1]);
		serverPort = (u_short)atoi(argv[2]);

		cout << "You request to service with ip: " << serverIP << " and port: " << serverPort << endl;
		
	}
	else {
		cout << "Invalid argument!" << endl;
		return -1;
	}

	WSAData wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData) != 0) {
		cout << "Winsock 2.2 is not supported" << endl;
		return -1;
	}
	//C:\\Users\\Admin\\Source\\Repos\\EncDecFileOEBClient\\test.txt
	string fileName = ASSESTS_DIR + string("test.txt");
	ifstream f(fileName.c_str());
	if (f.good()) {
		cout << "Check ok" << endl;
	}
	else {
		cout << "Check failed" << endl;
	}
	OverlappedConnection* pConn = new OverlappedConnection();
	if (!pConn->setServerAddress(serverIP, serverPort)) {
		cout << "Error: "<< ": Cannot init connection" << endl;
		pConn->freeResource();
		delete pConn;
		return -1;
	}

	EncDecClient* pClient = new EncDecClient();
	pClient->setConnection(pConn);

	EncDecFileController* pController = new EncDecFileController(pClient);

	UserCLI* pCLI = new UserCLI(pController);

	pCLI->loop();

	pConn->freeResource();
	delete pConn;
	delete pClient;
	delete pController;
	delete pCLI;
	WSACleanup();
	return 0;
}