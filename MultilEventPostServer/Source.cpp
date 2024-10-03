#include<iostream>
#include<string>
#include<string.h>
#include<queue>
#include<map>
#include<stdlib.h>
#include<sstream>
#include<fstream>
#include<process.h>
#include<Winsock2.h>
#include<Ws2tcpip.h>
#include<Windows.h>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_ADDR "127.0.0.1"
#define BUFFER_SIZE 2048
#define POST_SIZE 4096
#define ACC_FILE_PATH "account.txt"


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

// Ky tu phan tach
#define SPLIT_CHAR " "

// Danh dau tai khoan co bi khoa
#define LOCKED 1
#define UNLOCKED 0

// kich thuoc toi da lan luot cua header va content
#define HEADER_LENGTH 12
#define CONTENT_LENGTH 2048

// trang thai tra ve khi thuc hien 1 trao doi du lieu tren session
#define SUCCESS 1
#define FAIL 0
#define DEFAULT_CONTENT ""

//so socket toi da tren 1 luong
#define SOCKETS_PER_THREAD WSA_MAXIMUM_WAIT_EVENTS
//WSA_MAXIMUM_WAIT_EVENTS
// so luong toi da co the tao
#define LIMIT_THREAD 1024

// so socket trong hang doi 
#define LIMIT_WAIT_SOCKET 32
#define TIMEOUT_INTERVAL 20
// so maximum ket noi toi  tren listenSock
#define NUM_WAIT_CONN 64
#define PLANA
using namespace std;
#ifdef PLANA
// hang doi thong diep tren ket noi voi client
typedef struct {
	queue<string> _queue;
	string fragmented_message;
}MessageQueue;

// du lieu chua thong tin trang thai 1 tai khoan
typedef struct {
	string name;
	int status;
	int numLogin;
}State;

//hang doi ket noi chap nhan tu listenSock 
queue<pair<SOCKET, sockaddr_in>> connQueue;

// kieu du lieu chua ten tai khoan va trang thai
vector<State> accountList;


// mang theo doi 1 luong da qua tai so luong chua
bool isOverloads[LIMIT_THREAD];

u_short portServer;

// ham doc thong tin tai khoan
void readAccountFile(const char* f) {
	ifstream inFile;
	inFile.open(f);
	string line;


	while (getline(inFile, line)) {
		size_t split = line.find(" ");
		string userName = line.substr(0, split);
		int status = atoi(line.substr(split + 1).c_str());
		State s;
		s.numLogin = 0;
		s.status = status;
		s.name = userName;
		accountList.push_back(s);
	}

	inFile.close();
}

int findAccStateByName(string name) {
	int n = accountList.size();
	for (int i = 0; i < n; i++) {
		if (accountList[i].name == name) {
			return i;
		}
	}
	return -1;
}

// bien gang theo thu tu dieu do cac bien , cout(stream in ra man hinh), bien accountList, connQueue, overLoads
CRITICAL_SECTION cout_critical, accLst_critical, connQ_critical, isOverLoad_critical;

// lop dai dien cho 1 ket noi tcp voi 1 socket
//gui, nhan du lieu tang tcp
class Connection {
private:
	char clientIP[INET_ADDRSTRLEN];
	u_short clientPort;
	SOCKET connSock;
public:
	Connection(sockaddr_in addr, SOCKET connSock) {
		this->connSock = connSock;
		inet_ntop(AF_INET, &addr.sin_addr, clientIP, sizeof(clientIP));
		clientPort = ntohs(addr.sin_port);
	}

	SOCKET getSocket() {
		return connSock;
	}
	string getClientIP() {
		return string(clientIP);
	}

	u_short getClientPort() {
		return clientPort;
	}

	int read_data(char* buff, int length = BUFFER_SIZE) {
		int ret;
		char buffer[BUFFER_SIZE];
		ret = recv(connSock, buffer, BUFFER_SIZE, 0);
		EnterCriticalSection(&cout_critical);
		cout << "after read data" << endl;
		LeaveCriticalSection(&cout_critical);
		if (ret == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}
		else if (ret == 0) {
			return 0;
		}
		else {
			ret = ret > BUFFER_SIZE ? BUFFER_SIZE : ret;
			buffer[ret] = 0;
			strcpy_s(buff, length, buffer);
			return 1;
		}
	}

	int send_data(char* buff) {
		int ret;
		ret = send(connSock, buff, strlen(buff), 0);
		if (ret == SOCKET_ERROR) {
			return FAIL;
		}
		else {
			return SUCCESS;
		}
	}
	void closeSocket() {
		closesocket(connSock);
	}
};


// Dai dien cho 1 goi tin tang ung dung bao gom header va content, 
// giup phan tach header content hay nguoc lai dong goi thanh goi tin hoan chinh de gui
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

// Dai dien cho cac tac vu tang ung dung , 
// co cac chuc nang nhu xu ly login, logout, post 
// phan tach du lieu thanh cac thong diep hay goi tin tang ung dung
// luu trang thai dang nhap cua client
// su dung chuc nang cua cac lop duoi nhu connection de gui goi tin di 
class PostArticleSession {
private:
	Connection* pConn;
	string userName;
	bool isLogin;
	MessageQueue q;

	void forward_data_to_queue(char* data) {
		string dat = data;
		int n = dat.size();

		if (n == 0) {
			return;
		}
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

	void logTCPError(const char* msg) {
		EnterCriticalSection(&cout_critical);
		cout << "Client with address:(" << pConn->getClientIP() << ", " << pConn->getClientPort() << ")";
		if (isLogin) {
			cout << "and user name " << userName << endl;
		}
		cout << "Error " << WSAGetLastError() << " : " << msg << endl;
		LeaveCriticalSection(&cout_critical);
	}
	void logApp(const char* msg) {
		EnterCriticalSection(&cout_critical);
		cout << "Client: (" << pConn->getClientIP() << ", " << pConn->getClientPort() << "), ";
		if (isLogin) {
			cout << "user name:" << userName << "." << endl;
		}
		cout << msg << endl;
		LeaveCriticalSection(&cout_critical);
	}

	string processLogin(string name) {
		string header = NOT_DEFINE_MESSAGE;

		string msg = LOGIN;
		if (isLogin) {
			if (!userName.compare(name)) {
				header = SUCCESS_LOGIN;
				msg += " :Account is logged in before";
			}
			else {
				header = ANOTHER_LOGIN;
				msg += " :Client can't login with another account while is using the account";
			}
		}
		else {
			EnterCriticalSection(&cout_critical);
			cout<<"Before check login" <<endl;
			LeaveCriticalSection(&cout_critical);
			
			int idx = 0;
			EnterCriticalSection(&accLst_critical);
			idx = findAccStateByName(name);
			if (idx == -1) {
				header = NOT_EXIST_ACCOUNT;
				msg += " :The required account is not exist";
			}
			else {
				
				if (accountList[idx].status == LOCKED) {
					header = LOCKED_ACCOUNT;
					msg += " :The required account is locked";
				}
				else {
					header = SUCCESS_LOGIN;
					msg += " : Successful login with user name ";
					msg += name;
					
					accountList[idx].numLogin++;
					
					isLogin = true;
					userName = name;
				}
			}
			LeaveCriticalSection(&accLst_critical);

			EnterCriticalSection(&cout_critical);
			cout << "                                After check login" << endl;
			LeaveCriticalSection(&cout_critical);
		}
		logApp(msg.c_str());
		cout << DataPackage(header.c_str(), DEFAULT_CONTENT).getCompleteDataPackage() << endl;
		return DataPackage(header.c_str(), DEFAULT_CONTENT).getCompleteDataPackage();
	}

	string processPost(string article) {
		string msg = POST;
		string header = NOT_DEFINE_MESSAGE;
		msg += ": ";
		if (isLogin == false) {
			header = NOT_LOGIN;
			msg += "The client is not login to post article";
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
		string msg = "LOGOUT";

		msg += ": ";

		if (isLogin == false) {
			header = NOT_LOGIN;
			msg += "The client is not login";
		}
		else {
			EnterCriticalSection(&accLst_critical);
			int idx = 0;
			idx = findAccStateByName(userName);
			if (idx != -1) {
				accountList[idx].numLogin--;
			}
			LeaveCriticalSection(&accLst_critical);
			msg += userName;
			msg += " :Succesful logout.";
			isLogin = false;
			userName = "";
			header = SUCCESS_LOGOUT;
		}
		logApp(msg.c_str());
		return DataPackage(header.c_str(), DEFAULT_CONTENT).getCompleteDataPackage();
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

		return resp;
	}
public:
	PostArticleSession(SOCKET s, sockaddr_in addr) {
		pConn = new Connection(addr, s);
		userName = "";
		isLogin = false;
		q.fragmented_message = "";
	}
	// Thuc hien 1 dich vu ma client yeu cau
	// tra ve ma loi hanc thanh cong
	int serve() {
		// khai bao khoi tao cac bien can thiet
		int ret = 0;
		char buff[BUFFER_SIZE];
		string msg = "";

		ret = pConn->read_data(buff);

		if (ret == SOCKET_ERROR) {
			logTCPError("Cannot receive data");
			return FAIL;
		}
		else if (ret == 0) {
			logTCPError("Client disconnected!");
			return FAIL;
		}

		forward_data_to_queue(buff);

		while (!q._queue.empty()) {
			msg = q._queue.front();
			q._queue.pop();
			msg = createResponse(msg);
			strcpy_s(buff, msg.c_str());
			ret = pConn->send_data(buff);
			if (ret == FAIL) {
				logTCPError("Cannot send data");
				return FAIL;
			}
		}

		return SUCCESS;
	}

	void finish() {
		if (isLogin) {
			EnterCriticalSection(&accLst_critical);
			int idx = 0;
			idx = findAccStateByName(userName);
			if (idx != -1) {
				accountList[idx].numLogin--;
			}
			LeaveCriticalSection(&accLst_critical);
		}
		pConn->closeSocket();
		delete pConn;

	}
	~PostArticleSession() {
	}
};

// cau truc anh xa giua 1 socket voi 1 doi tuong lop PostArticleSession
// duoc su dung boi lop PostServer de quan ly cac phien giao tiep cua cac nguoi dung 
// Giup Post Server quan ly duoc 
typedef struct {
	SOCKET connSock;
	PostArticleSession* pSession;
}SessSockPair;

// Lop nay dai dien cho server thuc hien cong viec tren 1 luong 
// thuc hien viec theo doi cac phien dich vu
// lay cac ket noi trong hang doi va phuc vu client
// su dung ham select de theo doi cac yeu cau den tu cac client
class PostServer {
private:
	// bo sung listenSock cho viec lay cac thong diep
#ifdef HAS_LISTENSOCK
	SOCKET listenSock;
#endif
	// mang chua cac phien
	SessSockPair sessionArr[SOCKETS_PER_THREAD];
	WSAEVENT events[SOCKETS_PER_THREAD];
	unsigned int num_session;//theo doi so session tren server

	//loai bo 1 session
	void removeSession(int idx) {
		WSACloseEvent(events[idx]);
		events[idx] = nullptr;
		sessionArr[idx].connSock = 0;
		if (num_session > 0) {
			num_session--;
		}
		sessionArr[idx].pSession->finish();
		delete (sessionArr[idx].pSession);
		sessionArr[idx].pSession = 0;
	}

	// them 1 session neu duoc tre ve index < SOCKETS_PER_THREAD neu ko them thi tra ve SOCKETS_PER_THREAD
	int addSession(SOCKET conn, sockaddr_in addr, int begin = 0) {
		int i = SOCKETS_PER_THREAD;
		cout << "In addSession()" << endl;
		if (conn != 0 && num_session < SOCKETS_PER_THREAD) {
			for (i = begin; i < SOCKETS_PER_THREAD; i++) {
				if (sessionArr[i].connSock == 0) {
					if ((events[i] = WSACreateEvent()) == WSA_INVALID_EVENT) {
						cout << "in addSession: create invalid event" << endl;
						events[i] = nullptr;
						return i;
					}
					if (WSAEventSelect(conn, events[i], FD_READ | FD_CLOSE) == SOCKET_ERROR) {
						cout << "in addSession: select error" << endl;
						closesocket(conn);
						WSACloseEvent(events[i]);
						events[i] = nullptr;
						return i;
					}
					cout << "In addSession() ok" << endl;
					sessionArr[i].connSock = conn;
					sessionArr[i].pSession = new PostArticleSession(conn, addr);
					num_session++;
					break;
				}
			}
		}

		return i;
	}
public:

	// lay cac ket noi trong hang doi connQueue de thuic hien cong viec
	void retrieve_conn_from_queue() {
		/*cout << "In retrieve_conn_from_queue()" << endl;*/
		EnterCriticalSection(&connQ_critical);
		/*if (connQueue.empty()) {
			cout << "connQ is empty" << endl;
		}*/
		int first = 0;
		while (num_session < SOCKETS_PER_THREAD && (!connQueue.empty())) {
			/*cout << "In loop retrieve" << endl;*/
			pair<SOCKET, sockaddr_in> p = connQueue.front();
			first = addSession(p.first, p.second, first);
			if (first >= SOCKETS_PER_THREAD) {
				break;
			}
			first++;
			connQueue.pop();
		}
		LeaveCriticalSection(&connQ_critical);
	}

	PostServer(unsigned int num_sess = 0) {
		num_session = num_sess;

		for (int i = 0; i < SOCKETS_PER_THREAD; i++) {
			sessionArr[i].connSock = 0;
			sessionArr[i].pSession = NULL;
			events[i] = nullptr;
		}
		// bo sung khoi tao listenSock 

	}
	// don sach cac ket noi va phien tren  dua ve trnag thai moi khoi tao
	void refresh() {
		
		num_session = 0;
		for (int i = 0; i < SOCKETS_PER_THREAD; i++) {
			if (sessionArr[i].connSock != 0) {

				WSACloseEvent(events[i]);
				events[i] = 0;
				sessionArr[i].connSock = 0;
				sessionArr[i].pSession->finish();
				delete (sessionArr[i].pSession);
				sessionArr[i].pSession = NULL;
			}
		}
 

	}

	

	bool waitForRequestFromClients() {

		DWORD index = 0;

		index = WSAWaitForMultipleEvents(num_session, events, FALSE, 10, FALSE);
		/*cout << "After waitFor" << endl;*/
		if (index == WSA_WAIT_FAILED) {
			cout << "WSA_WAIT_FAILED" << endl;
			return false;
		}
		if (index == WSA_WAIT_TIMEOUT) {
			return true;
		}
		index = index - WSA_WAIT_EVENT_0;
		WSANETWORKEVENTS sockEvent;
		WSAEnumNetworkEvents(sessionArr[index].connSock, events[index], &sockEvent);
        if(sockEvent.lNetworkEvents & FD_READ){
			if (sockEvent.iErrorCode[FD_READ_BIT] != 0) {
				/*cout << "error read" << endl;*/
				return false;
			}
			/*cout << "In read" << endl;*/
			int ret = 0;
			ret = sessionArr[index].pSession->serve();
			WSAResetEvent(events[index]);
			if (ret == FAIL) {
				removeSession(index);
			}
			return true;
		}
		else if (sockEvent.lNetworkEvents & FD_CLOSE){
			if (sockEvent.iErrorCode[FD_CLOSE_BIT] != 0) {
				return false;
			}
			removeSession(index);
			return true;
		}
		return true;
	}

	unsigned int getNumSession() {
		return num_session;
	}
};

// luong thuc thi cong viec tiep nhan dich vu 
unsigned int __stdcall processBatchConnThread(void* param) {

	int n = *(int*)param;
	PostServer server(0);
	EnterCriticalSection(&cout_critical);
	cout << "Thread " << n << " is started." << endl;
	LeaveCriticalSection(&cout_critical);

	bool ret = false;
	bool tmpState = true;
	while (1) {
		// kiem tra server tren thread nay con dap ung duoc ket noi hay ko neu co thi thuc hien
		if (server.getNumSession() < SOCKETS_PER_THREAD) {
			server.retrieve_conn_from_queue();
		}
		// Danh dau lai vao isOveload la da full ket noi neu vay
		bool isOverLoad = !(server.getNumSession() < SOCKETS_PER_THREAD);
		if (tmpState != isOverLoad) {
			tmpState = isOverLoad;
			EnterCriticalSection(&isOverLoad_critical);
			isOverloads[n] = tmpState;
			LeaveCriticalSection(&isOverLoad_critical);
		}
		// phuc vu tat ca cac ket noi den tren 1 lan tham do bang ham select
		if (server.getNumSession() > 0) {
			//cout << "Thread " << n << ": in loop.if 3" << endl;
			ret = server.waitForRequestFromClients();
			// neu loi thi clean 
			if (ret == false) {
				server.refresh();
			}
		}
	}
	return 0;
}

// kiem tra tinh san sang phuic vu tren tat ca cac luong dang hoat dong
bool checkAvailableOfThreads(int num_thread) {
	bool check=false;
	int i = 0;
	if (num_thread <= 3) {
		return false;
	}
	EnterCriticalSection(&connQ_critical);
	check = connQueue.size() >= LIMIT_WAIT_SOCKET;
	LeaveCriticalSection(&connQ_critical);
	if (check) {
		return false;
	}
	for (i = 0, check=false; i < num_thread && !check; i++) {
		EnterCriticalSection(&isOverLoad_critical);
		if (isOverloads[i] == false) {
			check = true;
		}
		LeaveCriticalSection(&isOverLoad_critical);
	}
	

	return check;
}

int main(int argc, char** argv) {

	u_short portServer;

	portServer = (u_short)atoi(argv[1]);
	readAccountFile(ACC_FILE_PATH);


	// Thuc hien cong viec khoi tao winsock , 1 listensock tren luong chinh 
	// khoi tao mang isOverloads ve false 
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Winsock 2.2 is not supported" << endl;
		return 0;
	}
	SOCKET listenSock;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		cout << "Error " << WSAGetLastError() << ": Cannot listen socket at server" << endl;
		return 0;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(portServer);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error " << WSAGetLastError() << ": Cannot associate a local address with server socket." << endl;
		return 0;
	}

	if (listen(listenSock, NUM_WAIT_CONN)) {
		cout << "Error " << WSAGetLastError() << ": Cannot place server socket in state LISTEN." << endl;
		return  0;
	}

	SOCKET connSock;
	sockaddr_in clientAddr;
	int  clientAddrLen = sizeof(clientAddr);


	

	int num_thread = 0;
	// mang quan ly luong
	HANDLE threads[LIMIT_THREAD];

	for (int i = 0; i < LIMIT_THREAD; i++) {
		threads[i] = nullptr;
		isOverloads[i] = false;
	}

	cout << "Server started!" << endl;

	// khoi tao cac bien doan gang 
	InitializeCriticalSection(&cout_critical);
	InitializeCriticalSection(&accLst_critical);
	InitializeCriticalSection(&connQ_critical);
	InitializeCriticalSection(&isOverLoad_critical);

	/*
	* VOng lap
	* Thuc hien lang nghe tren listenSOck
	* chap nhan ket noi va dua vao connQueue
	* khoi tao them luong neu nhu tat ca cac luon con hien co dang busy
	*
	*/
	while (1) {
		
		// kiem tra neu loi thoat vong lap 
		
		// kiem tra xem co su kien tren listenSock
		clientAddrLen = sizeof(clientAddr);
		// chap nhan ket noi
		connSock = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
		if (connSock == SOCKET_ERROR) {
			EnterCriticalSection(&cout_critical);
			cout << "Error " << WSAGetLastError() << ": Cannot accept new connection" << endl;
			LeaveCriticalSection(&cout_critical);
			break;
		}
		else {
			// Dua ket noi vao connQueue
			
			EnterCriticalSection(&connQ_critical);
			if (connQueue.size() <= LIMIT_WAIT_SOCKET) {
				cout << "pass connect to connQ" << endl;
				connQueue.push(pair<SOCKET, sockaddr_in>(connSock, clientAddr));
			}
			else {
				closesocket(connSock);
			}
			LeaveCriticalSection(&connQ_critical);
		}
		connSock = 0;
		
		// kiem tra tinh san sang tren thread 
		// neu khong san sang khoai tao luong moi 
		if (!checkAvailableOfThreads(num_thread) && num_thread <= LIMIT_THREAD) {
			int idx = num_thread++;
			threads[idx] = (HANDLE)_beginthreadex(0, 0, processBatchConnThread, (void*)&idx, 0, 0);
		}
	}
	// Doi cho den khi tat cac cac luong deu ket thuc
	WaitForMultipleObjects(num_thread, threads, true, INFINITE);

	// xoa hay giai phong bien doan gamgg
	DeleteCriticalSection(&cout_critical);
	DeleteCriticalSection(&accLst_critical);
	DeleteCriticalSection(&connQ_critical);
	DeleteCriticalSection(&isOverLoad_critical);

	// dong ket noi va ket thuc 
	closesocket(listenSock);
	WSACleanup();
	return 0;
}
#endif