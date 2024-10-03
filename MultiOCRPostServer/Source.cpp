#include<iostream>
#include<string>
#include<string.h>
#include<queue>
#include<map>
#include<stdlib.h>
#include<sstream>
#include<fstream>
#include<process.h>
#include<WinSock2.h>
#include<Ws2tcpip.h>
#include<Windows.h>


#define SERVER_ADDR "127.0.0.1"
#define DATA_BUFSIZE 8192
#define RECEIVE 0
#define SEND 1

#define ENDING_DELIMITER "\r\n"

// Header phan hoi
#define SUCCESS_LOGIN "10"
#define LOCKED_ACCOUNT "11"
#define NOT_EXIST_ACCOUNT "12"
#define ANOTHER_LOGIN "14"
#define LOGGED_IN "13"
#define NOT_DEFINE_MESSAGE "99"
#define SUCCESS_POST "20"
#define NOT_LOGIN "21"
#define SUCCESS_LOGOUT "30"

// header yeu cau tu client
#define LOGIN "USER"
#define POST "POST"
#define LOGOUT "BYE"

// kich thuoc toi da lan luot cua header va content
#define HEADER_LENGTH 12
#define CONTENT_LENGTH 2048
#define DEFAULT_CONTENT ""

#define SPLIT_CHAR " "

#define RCV_CTRL 0
#define SEND_CTRL 1
#define SHUTDOWN_CTRL 2

#define MAX_VALUE_KEY 800
#define NAME_LENGTH 64

#define LOCKED 1
#define UNLOCKED 0

#define ACC_FILE_PATH "account.txt"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;
struct Account {
	int key;
	int status;
	int h;
	Account* left;
	Account* right;
	char name[NAME_LENGTH];
};

class AccountTree {
private:
	Account* pRoot;

	void update_height_node(Account* pNode) {

		if (pNode == NULL)
			return;

		if (pNode->left == NULL && pNode->right == NULL)
		{
			pNode->h = 1;
			return;
		}
		if (pNode->right == NULL) {
			pNode->h = pNode->left->h + 1;
			return;
		}
		if (pNode->left == NULL) {
			pNode->h = pNode->right->h + 1;
			return;
		}
		pNode->h = (pNode->left->h > pNode->right->h ? pNode->left->h : pNode->right->h) +1;
	}

	Account* rotateLeft(Account* pNode) {
		Account* pRight = pNode->right;
		if (pRight == NULL) {
			cout << "pRight is NULL in rotate" << endl;
			return pNode;
		}
		Account* pChildLeft = pRight->left;
		
		
		pRight->left = pNode;
		pNode->right = pChildLeft;

		update_height_node(pNode);
		update_height_node(pRight);
		
		return pRight;
	}

	Account* rotateRight(Account* pNode) {
		Account* pLeft = pNode->left;
		if (pLeft == NULL) {
			cout << "pLeft is NULL in rotate" << endl;
			return pNode;
		}
		
		Account* pChildRight = pLeft->right;

		pLeft->right = pNode;
		pNode->left = pChildRight;

		update_height_node(pNode);
		update_height_node(pLeft);

		return pLeft;
	}

	Account* balanceTree(Account* pNode) {
		Account* pLeft = NULL, *pRight = NULL;
		int hLeft = 0, hRight = 0 , hLeftChild = 0 , hRightChild = 0;

		if (pNode == NULL)
			return NULL;

		pLeft = pNode->left;
		pRight = pNode->right;
		if (pLeft == NULL && pRight == NULL)
			return pNode;

		if (pLeft == NULL) {
			if (pRight!=NULL && pRight->h >= 2) {
				cout << "";
				return rotateLeft(pNode);
			}
			return pNode;
		}
		if (pRight == NULL) {

			if(pLeft!=NULL && pLeft->h >= 2)
				cout << "" ;
				return rotateRight(pNode);

			return pNode;
		}
		hLeft = pNode->left->h;
		hRight = pNode->right->h;
		if (abs_sub(hLeft, hRight) <= 1)
			return pNode;
		if (hLeft > hRight) {
			hLeftChild = pLeft->left->h;
			hRightChild = pLeft->right->h;
			if (hRightChild > hLeftChild) {
				cout << "";
				pNode->left = rotateLeft(pLeft);
			}
			cout << "";
			return rotateRight(pNode);
		}
		hLeftChild = pRight->left->h;
		hRightChild = pRight->right->h;
		if (hLeftChild > hRightChild) {
			//cout << "after rotateRight: " << endl;
			pNode->right = rotateRight(pRight);
			
		}
		cout << "" ;
		return rotateLeft(pNode);
	}


	int abs_sub(int a, int b) {
		return a > b ? a - b : b - a;
	}
	int hash(const char* name, int n) {
		int hval = 0;
		for (int i = 0; i < n; i++) {
			hval = (hval + int(name[i])) % MAX_VALUE_KEY;
		}
		return hval;
	}
	bool addNode(Account* pNode, Account* parent) {
		if (pNode->key < parent->key) {
			if (parent->left == NULL) {
				parent->left = pNode;
				update_height_node(parent);
				//cout << "node: " << parent->name << " ,key: " << parent->key << " ,height: " << parent->h << endl;
				return true;
			}

			if (addNode(pNode, parent->left)) {
				//cout << "after balance Tree" << endl;
				parent->left = balanceTree(parent->left);
				update_height_node(parent);
				//cout << "node: " << parent->name << " ,key: " << parent->key << " ,height: " << parent->h << endl;
				return true;
			}
			else {
				return false;
			}
		}
		else if(pNode->key > parent->key) {
			if (parent->right == NULL) {
				parent->right = pNode;
				update_height_node(parent);
				//cout << "node: " << parent->name << " ,key: " << parent->key << " ,height: " << parent->h << endl;
				return true;
			}
			if (addNode(pNode, parent->right)) {

				//cout << "after balance Tree" << endl;
				parent->right = balanceTree(parent->right);
				update_height_node(parent);
				//cout << "node: " << parent->name << " ,key: " << parent->key << " ,height: " << parent->h << endl;
				return true;
			}
			else {
				return false;
			}
		}
		else {
			if (!strcmp(pNode->name, parent->name)) {
				return false;
			}
			else {
				if (parent->left == NULL) {
					parent->left = pNode;
					update_height_node(parent);
					//cout << "node: " << parent->name << " ,key: " << parent->key << " ,height: " << parent->h << endl;
					return true;
				}
				if (addNode(pNode, parent->left)) {

					//cout << "after balance Tree" << endl;
					parent->left = balanceTree(parent->left);
					update_height_node(parent);
					//cout << "node: " << parent->name << " ,key: " << parent->key << " ,height: " << parent->h << endl;
					return true;
				}
				else {
					return false;
				}
			}
			
		}
		return false;
	}
	
	Account* find_successor(Account* pNode) {
		if (pNode->left == NULL)
			return NULL;
		return most_right(pNode->left);
	}

	Account* most_right(Account* pNode) {
		if (pNode->right == NULL) {
			return pNode;
		}
		return most_right(pNode->right);
	}
	Account* most_left(Account* pNode) {
		if (pNode->left == NULL) {
			return pNode;
		}
		return most_left(pNode->left);
	}

	void updateNode(Account* pNode) {
		if (pNode == NULL)
			return;
		if (pNode->left == NULL && pNode->right == NULL)
			return;
		updateNode(pNode->left);
		updateNode(pNode->right);
		pNode->left = balanceTree(pNode->left);
		pNode->right = balanceTree(pNode->right);
		update_height_node(pNode);
	}


public:
	AccountTree() {
		pRoot = (Account*)malloc(sizeof(Account));
		if (pRoot != NULL) {
			strcpy_s(pRoot->name, NAME_LENGTH, "default*^5655");
			pRoot->key = MAX_VALUE_KEY/2;
			pRoot->status = LOCKED;
			pRoot->left = NULL;
			pRoot->right = NULL;
			pRoot->h = 1;
		}
	}
	bool destroyBranch(Account* pNode) {
		bool check = true;
		if (pNode == NULL) {
			return true;
		}
		if (pNode->left != NULL) {
			check = check && destroyBranch(pNode->left);
		}
		if (pNode->right != NULL) {
			check = check && destroyBranch(pNode->right);
		}
		free(pNode);
		return check;
	}

	bool insert(const char* name, int status) {
		int n = strlen(name);
		if (n > NAME_LENGTH)
			return false;
		Account* pNew = (Account*)malloc(sizeof(Account));
		if (pNew == NULL)
			return false;
		strcpy_s(pNew->name, NAME_LENGTH, name);
		pNew->status = status;
		pNew->key = hash(pNew->name, n);
		pNew->left = NULL;
		pNew->right = NULL;
		pNew->h = 1;
		if (addNode(pNew, pRoot)) {
			//cout << "insert is ok" << endl;
			pRoot = balanceTree(pRoot);
			//cout << "new pRoot: " << pRoot->name << " ,key: " << pRoot->key << " ,height: " << pRoot->h << endl;
			return true;
		}
		else {
			free(pNew);
			return false;
		}
	}

	bool remove(const char* name) {
		int key = hash(name, (int)strlen(name));
		bool isLeft = false;
		Account* pNode = pRoot;
		Account* pParent = pRoot;

		while (pNode != NULL) {
			if (key == pNode->key) {
				if (!strcmp(pNode->name, name)) {
					break;
				}
				else {
					pParent = pNode;
					pNode = pNode->left;
					isLeft = true;
					
				}
			}
			else if (key < pNode->key) {
				pParent = pNode;
				pNode = pNode->left;
				isLeft = true;
				
			}
			else {
				pParent = pNode;
				pNode = pNode->right;
				isLeft = false;
				
			}
		}

		if (pNode == NULL) {
			return true;
		}
		Account* pSuccessor = find_successor(pNode);

		if (pSuccessor == NULL) {
			cout << "pSuccessor is NULL" << endl;
			if (isLeft) {
				pParent->left = pNode->right;
			}
			else {
				pParent->right = pNode->right;
			}

			free(pNode);
		}
		else {
			cout << "replace pNode by value of pSuccessor " << endl;
			pNode->key = pSuccessor->key;
			strcpy_s(pNode->name, pSuccessor->name);
			pNode->status = pSuccessor->status;

			pParent = pNode->left;
			if (pParent == pSuccessor) {
				pNode->left = pSuccessor->left;
			}
			else {
				cout << "inwhile find to parent of pSuccessor" << endl;
				while (pParent->right != pSuccessor) {
					pParent = pParent->right;
				}
				pParent->right = pSuccessor->left;
			}
			free(pSuccessor);
		}

		cout << "aftet updateNode" << endl;
		updateNode(pRoot);
		pRoot = balanceTree(pRoot);
		return true;
	}
	Account* search(const char* name) {
		int key = hash(name, (int)strlen(name));
		Account* pTmp = pRoot;

		while (pTmp != NULL) {
			if (key == pTmp->key) {
				if (!strcmp(pTmp->name, name)) {
					return pTmp;
				}
				else {
					pTmp = pTmp->left;
				}
			}
			else if(key < pTmp->key){
				pTmp = pTmp->left;
			}
			else {
				pTmp = pTmp->right;
			}
		}
		return NULL;
	}

	bool freeResource() {
		return destroyBranch(pRoot);
	}
};

AccountTree* pTree;
char serverIP[INET_ADDRSTRLEN];
u_short serverPort = 5500;
void readAccountFile(const char* f) {
	ifstream inFile;
	inFile.open(f);
	string line;

	while (getline(inFile, line)) {

		size_t split = line.find(" ");
		string userName = line.substr(0, split);
		
		int status = atoi(line.substr(split + 1).c_str());
		//cout << "user name: " << userName << " status: "<<status << endl;
		pTree->insert(userName.c_str(), status);
	}

	inFile.close();
}

CRITICAL_SECTION accTree_critical, cout_critical;

class ApplicationInterface {
public:
	virtual int getControl() {
		return 0;
	}
	virtual bool forwardData(char* data, int length) {
		return true;
	}

	virtual void processData() {}

	virtual bool retrieveData(char outDat[], LPDWORD length) {
		return true;
	}

	virtual void finish(){}
};

class DataPackage {
private:
	char header[HEADER_LENGTH];
	char content[CONTENT_LENGTH];
	bool isDefine;
public:
	DataPackage(string data) {
		isDefine = true;
		size_t split_idx = 0;
		strcpy_s(header, HEADER_LENGTH, NOT_DEFINE_MESSAGE);
		strcpy_s(content, CONTENT_LENGTH, DEFAULT_CONTENT);
		split_idx = data.find(SPLIT_CHAR);
		if (split_idx != string::npos) {
			strcpy_s(header, HEADER_LENGTH, data.substr(0, split_idx).c_str());
			strcpy_s(content, CONTENT_LENGTH, data.substr(split_idx + 1).c_str());
		}
		else {
			isDefine = false;
			/*if (!data.substr(0, 3).compare(LOGOUT)) {
				strcpy_s(header, LOGOUT);
				strcpy_s(content, "");
			}
			else {
				isDefine = false;
			}*/
		}

		

	}
	bool isValid() {
		return isDefine;
	}
	DataPackage(const char* header, const char* content) {
		strcpy_s(this->header, HEADER_LENGTH, header);
		strcpy_s(this->content, CONTENT_LENGTH, content);
		if (strlen(this->header) > 0) {
			isDefine = true;
		}
		else {
			isDefine = false;
		}
	}

	string getHeader() {
		return string(header);
	}

	string getContent() {
		return string(content);
	}

	string getCompleteDataPackage() {
		stringstream ss;

		if (!isDefine) {
			strcpy_s(header, HEADER_LENGTH, NOT_DEFINE_MESSAGE);
		}

		ss << header;
		ss << SPLIT_CHAR;
		ss << content;
		ss << ENDING_DELIMITER;

		return ss.str();
	}
};

typedef struct {
	queue<string> _queue;
	string fragmented_message;
}MessageQueue;

class PostSession :public ApplicationInterface {
private:
	string userName;
	bool isLogin;
	MessageQueue q;
	queue<string> resp_q;
	int ctrl;
	int willShutdown;
	void logApp(const char* msg) {
		EnterCriticalSection(&cout_critical);
		//cout << "Client: (" << pConn->getClientIP() << ", " << pConn->getClientPort() << "), ";
		if (isLogin) {
			cout << "user name:" << userName << "." << endl;
		}
		cout << msg << endl;
		LeaveCriticalSection(&cout_critical);
	}

	void forward_data_to_queue(char* data) {
		string dat = data;
		int n = dat.size();

		if (n == 0) {
			return;
		}
		cout << "forward_data_to_queue: " << dat<< endl;
		size_t first = 0, last = 0;
		size_t len = q.fragmented_message.size();
		if (len > 0 && q.fragmented_message[len - 1] == '\r' && dat[0] == '\n') {
			q._queue.push(q.fragmented_message.substr(0, len - 1));
			q.fragmented_message = "";
			first++;
		}

		while (first != string::npos && (int)first < n) {
			string tmp(q.fragmented_message);
			last = dat.find(ENDING_DELIMITER, first);

			if (last == string::npos) {
				q.fragmented_message += dat.substr(first);
				first = last;
			}
			else {
				tmp += dat.substr(first, last - first);
				q.fragmented_message = "";
				q._queue.push(tmp);
				first = last + 2;
			}
		}

	}
	string processLogin(string name) {
		string header = NOT_DEFINE_MESSAGE;
		Account* pAcc = NULL;
		
		if (isLogin) {
			if (!userName.compare(name)) {
				header = SUCCESS_LOGIN;
				
			}
			else {
				header = ANOTHER_LOGIN;
				
			}
		}
		else {
			
			EnterCriticalSection(&accTree_critical);
			pAcc = pTree->search(name.c_str());
			LeaveCriticalSection(&accTree_critical);

			if (pAcc == NULL) {
				header = NOT_EXIST_ACCOUNT;
			}
			else {

				if (pAcc->status == LOCKED) {
					header = LOCKED_ACCOUNT;
					//msg += " :The required account is locked";
				}
				else {
					header = SUCCESS_LOGIN;
					//msg += " : Successful login with user name ";
					//msg += name;

					//accountList[idx].numLogin++;

					isLogin = true;
					userName = name;
				}
			}
			

			
		}
		//logApp(msg.c_str());
		cout << DataPackage(header.c_str(), DEFAULT_CONTENT).getCompleteDataPackage() << endl;
		return DataPackage(header.c_str(), DEFAULT_CONTENT).getCompleteDataPackage();
	}

	string processPost(string article) {
		string msg = POST;
		string header = NOT_DEFINE_MESSAGE;
		msg += ": ";
		if (isLogin == false) {
			header = NOT_LOGIN;
			//msg += "The client is not login to post article";
		}
		else {
			header = SUCCESS_POST;
			msg += "OK article:";
			msg += article;
		}
		logApp(msg.c_str());
		return DataPackage(header.c_str(), DEFAULT_CONTENT).getCompleteDataPackage();
	}

	string processLogout() {
		string header = NOT_DEFINE_MESSAGE;
		string msg = "LOGOUT: ";
		
		

		if (isLogin == false) {
			header = NOT_LOGIN;
			msg += "The client is not login";
		}
		else {
			msg += userName;
			msg += " :Succesful logout.";
			isLogin = false;
			userName = "";
			header = SUCCESS_LOGOUT;
			willShutdown++;
		}
		logApp(msg.c_str());
		return header+ENDING_DELIMITER;
	}

	string createResponse(string msg) {
		DataPackage pack(msg);
		string resp = DataPackage(NOT_DEFINE_MESSAGE, DEFAULT_CONTENT).getCompleteDataPackage();

		if (!pack.isValid()) {
			return resp;
		}
		string header = pack.getHeader();
		if (!header.compare(LOGIN)) {
			string name = pack.getContent();
			resp = processLogin(name);
		}
		else if (!header.compare(POST)) {
			string article = pack.getContent();
			resp = processPost(article);
		}
		else if (!header.compare(LOGOUT)) {
			resp = processLogout();
		}
		cout << "In createResponse: " << resp << endl;
		return resp;
	}
	
public:
	PostSession() {
		userName = "";
		isLogin = false;
		q.fragmented_message = "";
		ctrl = RCV_CTRL;
		willShutdown = 0;
	}
	
	int getControl() {

		
		if (willShutdown == 1) {
			willShutdown++;
			return ctrl;
		}
		if (willShutdown >= 2) {
			return SHUTDOWN_CTRL;
		}
		return ctrl;
	}

	bool forwardData(char* data, int length) {
		data[length] = 0;
		cout << "forward data: " << data << endl;
		forward_data_to_queue((char*)data);
		return true;
	}
	bool retrieveData(char outDat[], LPDWORD length) {

		string resp = "";
		string tmp = "";
		*length = 0;
		if (resp_q.empty()) {
			return false;
		}
		
		while (!resp_q.empty()) {

			tmp = resp_q.front();
			if (resp.size() + tmp.size() > DATA_BUFSIZE) {
				break;
			}
			resp_q.pop();
			resp += tmp;
			
		}
		*length = (DWORD)resp.size();
		for (int i = 0; i < (int)resp.size(); i++) {
			outDat[i] = resp[i];
		}
		cout << "retrieve data: " << resp << endl;
		return true;
	}

	void processData() {
		string inp_dat = "";
		while (!q._queue.empty()) {
			inp_dat = q._queue.front();
			resp_q.push(createResponse(inp_dat));
			q._queue.pop();
		}
		if (ctrl != SHUTDOWN_CTRL) {
			if (resp_q.empty()) {
				ctrl = RCV_CTRL;
			}
			else {
				ctrl = SEND_CTRL;
			}
		}
	}

	void finish() {
		ctrl = SHUTDOWN_CTRL;
	}
};


typedef struct {
	WSAOVERLAPPED overlapped;
	WSABUF dataBuff;
	CHAR buffer[DATA_BUFSIZE];
	ApplicationInterface* pSess;
	DWORD recvBytes;
	DWORD sentBytes;
	int operation;
}PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;

typedef struct {
	SOCKET socket;
}PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

void freeSession(LPPER_HANDLE_DATA lpHdData, LPPER_IO_OPERATION_DATA lpOpData) {
	closesocket(lpHdData->socket);
	GlobalFree(lpHdData);
	lpOpData->pSess->finish();
	delete lpOpData->pSess;
	GlobalFree(lpOpData);
}

unsigned __stdcall serverWorkerThread(LPVOID completionPortID)
{
	HANDLE completionPort = (HANDLE)completionPortID;
	DWORD transferredBytes;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	DWORD flags=0;
	int ctrlKey;
	while (TRUE) {
		if (GetQueuedCompletionStatus(completionPort, &transferredBytes, (LPDWORD)&perHandleData, (LPOVERLAPPED*)&perIoData, INFINITE) == 0)
		{
			cout << "GetQueuedCompletionStatus() failed with error" << WSAGetLastError() << endl;
			return 0;
		}
		if (transferredBytes == 0) {
			freeSession(perHandleData, perIoData);
			continue;
		}

		if (perIoData->operation == RECEIVE) {
			perIoData->recvBytes = transferredBytes;
			perIoData->sentBytes = 0;
			perIoData->pSess->forwardData(perIoData->dataBuff.buf, perIoData->recvBytes);
		}

		perIoData->pSess->processData();
		ctrlKey = perIoData->pSess->getControl();

		if (ctrlKey == SHUTDOWN_CTRL) {
			freeSession(perHandleData, perIoData);
		}
		else if (ctrlKey == RCV_CTRL) {
			perIoData->recvBytes = 0;
			perIoData->operation = RECEIVE;
			flags = 0;
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->dataBuff.len = DATA_BUFSIZE;
			perIoData->dataBuff.buf = perIoData->buffer;
			if (WSARecv(perHandleData->socket,
				&(perIoData->dataBuff),
				1,
				&perIoData->recvBytes,
				&flags,
				&(perIoData->overlapped), NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					freeSession(perHandleData, perIoData);
					cout << "WSARecv() failed with error " << WSAGetLastError() << endl;

				}
			}
		}
		else if (ctrlKey == SEND_CTRL) {
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->pSess->retrieveData(perIoData->buffer, &perIoData->sentBytes);
			perIoData->dataBuff.buf = perIoData->buffer;
			perIoData->dataBuff.len = perIoData->sentBytes;
			perIoData->operation = SEND;

			if (WSASend(perHandleData->socket,
				&(perIoData->dataBuff),
				1,
				&perIoData->sentBytes,
				flags,
				&(perIoData->overlapped),
				NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					freeSession(perHandleData, perIoData);
					cout << "WSASend() failed with error " << WSAGetLastError() << endl;
				}
			}
		}
	}
}

int main(int argc, char** argv) {
	SOCKADDR_IN serverAddr, clientAddr;
	SOCKET listenSock, acceptSock;
	HANDLE completionPort;
	SYSTEM_INFO systemInfo;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	DWORD transferredBytes;
	DWORD flags;
	WSADATA wsaData;
	DWORD num_threads = 0;
	HANDLE threads[64];
	int clientAddrLen = sizeof(clientAddr);
	
	if (WSAStartup((2, 2), &wsaData) != 0) {
		cout << "WSAStartup() failed with error " << WSAGetLastError() << endl;
		return -1;
	}
	completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (completionPort == NULL) {
		cout << "CreateIoCompletionPort() failed with error " <<GetLastError() <<endl;
		WSACleanup();
		return -1;
	}

	serverPort = (u_short)atoi(argv[1]);
	strcpy_s(serverIP, INET_ADDRSTRLEN, SERVER_ADDR);
	pTree = new AccountTree();
	readAccountFile(ACC_FILE_PATH);
	
	GetSystemInfo(&systemInfo);

	num_threads = systemInfo.dwNumberOfProcessors * 2;
	for (int i = 0; i < (int)num_threads; i++) {
		if ((threads[i] =(HANDLE)_beginthreadex(0, 0, serverWorkerThread, (LPVOID)completionPort, 0, 0)) == 0) {
			cout << "create thread failed with error " << GetLastError() << endl;
			WSACleanup();
			pTree->freeResource();
			delete pTree;
			return -1;
		}
	}

	listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSock == INVALID_SOCKET) {
		cout << "WSASocket() failed with error " << WSAGetLastError() << endl;
		WSACleanup();
		pTree->freeResource();
		delete pTree;
		return -1;
	}
	
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
	if (bind(listenSock, (PSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "bind() failed with error " << WSAGetLastError() << endl;
		WSACleanup();
		closesocket(listenSock);
		pTree->freeResource();
		delete pTree;
		return -1;
	}

	if (listen(listenSock, 20) == SOCKET_ERROR) {
		cout << "listen() failed with error." << endl;
		WSACleanup();
		closesocket(listenSock);
		pTree->freeResource();
		delete pTree;
		return -1;
	}

	InitializeCriticalSection(&accTree_critical);
	InitializeCriticalSection(&cout_critical);
	cout << "Server started!" << endl;
	while (1) {
		acceptSock = 0;
		perHandleData = NULL;
		perIoData = NULL;
		acceptSock = WSAAccept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen, NULL, NULL);
		if (acceptSock == SOCKET_ERROR) {
			cout << "WSAAccept() failed with error " << WSAGetLastError() << endl;
			break;
		}
		cout << "accept connection" << endl;
		perHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));
		if (perHandleData == NULL) {
			closesocket(acceptSock);
			cout << "GloabalAlloc() failed with error " << GetLastError() << endl;
			break;
		}
		perHandleData->socket = acceptSock;
		if (CreateIoCompletionPort((HANDLE)acceptSock, completionPort, (DWORD)perHandleData, 0) == NULL)
		{
			closesocket(acceptSock);
			GlobalFree(perHandleData);
			cout << "CreateIoCompletionPort() failed with error " << WSAGetLastError() << endl;
			break;
		}

		perIoData = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATION_DATA));
		if (perIoData == NULL) {
			closesocket(acceptSock);
			GlobalFree(perHandleData);
			cout << "GlobalAlloc() failed with error " << GetLastError() << endl;
			break;
		}
		ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
		perIoData->sentBytes = 0;
		perIoData->recvBytes = 0;
		perIoData->dataBuff.len = DATA_BUFSIZE;
		perIoData->dataBuff.buf = perIoData->buffer;
		perIoData->operation = RECEIVE;
		// ti sua cho nay
		perIoData->pSess = new PostSession();
		flags = 0;

		if (WSARecv(acceptSock, &(perIoData->dataBuff), 1, &transferredBytes, &flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != ERROR_IO_PENDING) {
				closesocket(acceptSock);
				GlobalFree(perHandleData);
				perIoData->pSess->finish();
				GlobalFree(perIoData);
				cout << "WSARecv() failed with error " << WSAGetLastError() << endl;
				break;
			}
		}
	}
	WaitForMultipleObjects(num_threads, threads, true, INFINITE);
	DeleteCriticalSection(&accTree_critical);
	DeleteCriticalSection(&cout_critical);
	// Doi cho tat ca cac luong hoan thanh 
	closesocket(listenSock);
	pTree->freeResource();
	delete pTree;
	WSACleanup();
	return 0;
}

