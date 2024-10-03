#include<iostream>
#include<string>
#include<string.h>
#include<stdlib.h>
#include<sstream>
#include<Winsock2.h>
#include<Ws2tcpip.h>
#include<limits>
#include<queue>

#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_SIZE 2048
#define POST_SIZE 4096

#define ENDING_DELIMITER "\r\n"
#define SPLIT_CHAR " "

#define SUCCESS_LOGIN "10"
#define LOCKED_ACCOUNT "11"
#define NOT_EXIST_ACCOUNT "12"
#define ANOTHER_LOGIN "14"
#define LOGGED_IN "13"
#define NOT_DEFINE_MESSAGE "99"
#define SUCCESS_POST "20"
#define NOT_LOGIN "21"
#define SUCCESS_LOGOUT "30"

#define HEADER_LENGTH 16
#define CONTENT_LENGTH 10000

#define NAME_LENGTH 41
#define POST_LENGTH 2001

#define RESP_LENGTH 41

#define TCP_ERROR -1
#define SUCCESS 1
#define FAIL 0

#define LOGIN "USER"
#define POST "POST"
#define LOGOUT "BYE"

#define WRONG_USERNAME 2
#define INOT_LOGIN 3
#define ILOGGED_IN 4
#define INOT_DF_MSG 5
#define ILOCKED_ACC 6
#define IANOTHER_LOGIN 7
#define SERVER_DISCONNECT 8
#define ANOTHER_ERROR 9


#define WRONG_KEY -1
using namespace std;


void printError(const char* msg) {
	int errorCode = WSAGetLastError();
	if (errorCode == WSAETIMEDOUT) {
		cout << "Time out" << endl;
	}
	cout << "Error: " << errorCode << " " << msg << endl;
}

class ClientRequest {
private:
	char header[HEADER_LENGTH];
	char content[CONTENT_LENGTH];
public:
	ClientRequest(const char* type, const char* data) {
		stringstream ss;
		ss << type;

		strcpy_s(header, ss.str().c_str());
		strcpy_s(content, data);
	}
	ClientRequest() {

		strcpy_s(content, sizeof content, "");
		strcpy_s(header, "");
	}

	void setType(const char* type) {
		strcpy_s(header, type);
	}

	void setContent(const char* data) {
		strcpy_s(content, sizeof content, data);
	}
	void getDataPackage(char* datapack, int length = BUFFER_SIZE) {
		cout << "Header: " << header << endl;
		cout << "Content: " << content << endl;
		if (strlen(content) == 0 || strlen(header) == 0) {
			return;
		}


		stringstream ss;
		ss << header;
		ss << SPLIT_CHAR;
		if (strlen(content) > 0) {
			ss << content;
		}
		ss << ENDING_DELIMITER;
		strcpy_s(datapack, length, ss.str().c_str());
	}



};


class PostArticleClient {
private:
	SOCKET s;


public:
	PostArticleClient(SOCKET s) {
		this->s = s;
	}

	int sendMessage(ClientRequest& req) {
		char buff[BUFFER_SIZE];
		req.getDataPackage(buff);
		int ret;
		int length = strlen(buff);

		cout << "Length data: " << length << endl;
		cout << "Package: " << buff << endl;
		ret = send(s, buff, length, 0);
		if (ret == SOCKET_ERROR) {
			return ret;
		}
		else if (ret == 0) {
			return 0;
		}
		else {
			return 1;
		}

	}

	int recvMessage(char* resp, int length) {
		int ret;

		ret = recv(s, resp, length, 0);

		if (ret == SOCKET_ERROR) {
			return ret;
		}
		else if (ret == 0) {
			return 0;
		}
		else {
			resp[ret] = 0;
			return 1;
		}
	}

	int login(const char* name) {
		ClientRequest req(LOGIN, name);
		char buff[BUFFER_SIZE];
		int ret;
		ret = sendMessage(req);
		if (ret == SOCKET_ERROR) {
			return TCP_ERROR;
		}
		else if (ret == 0) {
			return SERVER_DISCONNECT;
		}
		else {
			ret = recvMessage(buff, BUFFER_SIZE);
			if (ret == SOCKET_ERROR) {
				return TCP_ERROR;
			}
			else if (ret == 0) {
				return SERVER_DISCONNECT;
			}
			else {
				int n = strlen(buff);
				if (n > 2) {
					buff[n - 2] = 0;
					if (!strcmp(buff, SUCCESS_LOGIN)) {
						return SUCCESS;
					}
					else if (!strcmp(buff, LOCKED_ACCOUNT)) {
						return ILOCKED_ACC;
					}
					else if (!strcmp(buff, NOT_EXIST_ACCOUNT)) {
						return WRONG_USERNAME;
					}
					else if (!strcmp(buff, LOGGED_IN)) {
						return FAIL;
					}
					else if (!strcmp(buff, ANOTHER_LOGIN)) {
						cout << "You logged in with another account" << endl;
						return WRONG_USERNAME;
					}
					else if (!strcmp(buff, NOT_DEFINE_MESSAGE)) {
						return INOT_DF_MSG;
					}
				}

			}
		}
		return ANOTHER_ERROR;
	}

	int postArticle(const char* post) {
		ClientRequest req(POST, post);
		int ret;
		char buff[BUFFER_SIZE];
		ret = sendMessage(req);

		if (ret == SOCKET_ERROR) {
			return TCP_ERROR;
		}
		else if (ret == 0) {
			return SERVER_DISCONNECT;
		}
		else {
			ret = recvMessage(buff, BUFFER_SIZE);
			if (ret == SOCKET_ERROR) {
				return SOCKET_ERROR;
			}
			else if (ret == 0) {
				return SERVER_DISCONNECT;
			}
			else {
				ret = strlen(buff);
				if (ret >= 2) {
					buff[ret - 2] = 0;
					if (!strcmp(buff, SUCCESS_POST)) {
						return SUCCESS;
					}
					else if (!strcmp(buff, NOT_LOGIN)) {
						return INOT_LOGIN;
					}
					else if (!strcmp(buff, NOT_DEFINE_MESSAGE)) {
						return INOT_DF_MSG;
					}
				}
			}
		}
		return ANOTHER_ERROR;
	}

	int logout() {
		ClientRequest req(LOGOUT, "BYE");
		int ret;
		char buff[BUFFER_SIZE];
		ret = sendMessage(req);

		if (ret == SOCKET_ERROR) {
			return TCP_ERROR;
		}
		else if (ret == 0) {
			return SERVER_DISCONNECT;
		}
		else {
			ret = recvMessage(buff, BUFFER_SIZE);
			if (ret == SOCKET_ERROR) {
				return TCP_ERROR;
			}
			else if (ret == 0) {
				return SERVER_DISCONNECT;
			}
			else {
				ret = strlen(buff);
				if (ret >= 2) {
					buff[ret - 2] = 0;
					if (!strcmp(buff, SUCCESS_LOGOUT)) {
						return SUCCESS;
					}
					else if (!strcmp(buff, NOT_LOGIN)) {
						return INOT_LOGIN;
					}
					else if (!strcmp(buff, NOT_DEFINE_MESSAGE)) {
						return INOT_DF_MSG;
					}
				}
			}
		}
		return ANOTHER_ERROR;
	}

};


class UserInterface {
private:
	PostArticleClient* pClient;
	int enterCtrlkey() {

		int key;
		char k;
		cout << "Press random key to continue." << endl;
		cin.ignore(100, '\n');
		cout << "Press key 0,1, 2 and 3 to select control" << endl;
		cin >> k;

		key = int(k) - int('0');
		if (key < 0 || key >= 4) {
			cout << "Wrong key. Please press key follow guide" << endl;
			return WRONG_KEY;
		}
		else {
			return key;
		}
	}
	int interactLogin() {
		string buff;
		int ret = 0;
		cout << "You are required to login" << endl;
		cout << "Please enter your username " << endl;
		cout << "UserName: " << endl;
		cin.ignore();
		getline(cin, buff);
		cout << endl;

		ret = pClient->login(buff.c_str());
		if (ret == SUCCESS) {
			cout << "Server: ";
			cout << "Successful login!" << endl;
			cout << "Hello " << buff << endl;
			cout << "Welcome to the posting service" << endl;

		}
		else if (ret == TCP_ERROR || ret == SERVER_DISCONNECT) {
			cout << "Server: ";
			cout << "Ohh! Our service is having problems" << endl;
			cout << "Sorry for this problem" << endl;
			return 0;
		}
		else if (ret == ILOCKED_ACC) {
			cout << "Server: ";
			cout << "This account has been locked" << endl;

		}
		else if (ret == WRONG_USERNAME) {
			cout << "Server: ";
			cout << "Invalid username or you logged in by another account" << endl;

		}
		else if (ret == FAIL) {
			cout << "Server: ";
			cout << "This account is already logged in somewhere else" << endl;
		}
		else {
			cout << "Server: ";
			cout << "Another error" << endl;
		}
		return 1;
	}
	int interactLogout() {
		int ret = 1;
		cout << "You are required to logout " << endl;

		ret = pClient->logout();
		if (ret == TCP_ERROR || ret == SERVER_DISCONNECT) {
			cout << "Server: ";
			cout << "Ohh! Our service is having problems" << endl;
			cout << "Sorry for this problem" << endl;
			return 0;
		}
		else if (ret == SUCCESS) {
			cout << "Server: ";
			cout << "Successful logout" << endl;
		}
		else if (ret == INOT_LOGIN) {
			cout << "Server: ";
			cout << "You don't login to logout" << endl;
		}
		else {
			cout << "Server: ";
			cout << "Another problem" << endl;
		}

		return 1;
	}
	int interactPost() {
		int ret = 1;
		string buff;
		cout << "You are reuqired to post" << endl;
		cout << "Let's enter below a pragraph have no more 2000 words to post to server" << endl;
		cin.ignore();
		getline(cin, buff);
		ret = pClient->postArticle(buff.c_str());
		if (ret == TCP_ERROR || ret == SERVER_DISCONNECT) {
			cout << "Server: ";
			cout << "TCP ERROR" << endl;
			cout << "Ohh! Our service is having problems" << endl;
			cout << "Sorry for this problem" << endl;

			return 0;
		}
		else if (ret == SUCCESS) {
			cout << "Server: ";
			cout << "Successful post" << endl;

		}
		else if (ret == INOT_LOGIN) {
			cout << "Server: ";
			cout << "Ohh! You don't login" << endl;
			cout << "Lets login before article" << endl;
		}
		else {
			cout << "Server: ";
			cout << "Another problem" << endl;
		}

		return 1;
	}

	void sayGoodBye() {
		cout << "Thanks you for use Our service" << endl;
		cout << "Bye bye. See you again" << endl;
	}
public:
	UserInterface(PostArticleClient* pClient) {
		this->pClient = pClient;
	}
	void displayGuide() {
		cout << "Welcome to post article service" << endl;
		cout << "Guide:" << endl;
		cout << "Press 0: Login" << endl;
		cout << "Press 1: Post Article" << endl;
		cout << "Press 2: Logout" << endl;
		cout << "Press 3: Finish" << endl;
		cout << "You should to login before post article" << endl;
	}

	void loop() {
		while (1) {
			int check = 1;
			int key = enterCtrlkey();

			if (key == WRONG_KEY) {
				continue;
			}
			if (key == 0) {
				check = interactLogin();
			}
			else if (key == 1) {
				check = interactPost();
			}
			else if (key == 2) {
				check = interactLogout();
			}
			else if (key == 3) {
				sayGoodBye();
				break;
			}
			if (check == 0) {
				break;
			}
		}
	}
};


int main(int argc, char** argv) {

	char serverIP[INET_ADDRSTRLEN];
	u_short serverPort;
	strcpy_s(serverIP, argv[1]);
	serverPort = (u_short)atoi(argv[2]);
	cout << "Server with address (" << serverIP << ", " << serverPort << ")" << endl;
	WSAData wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		cout << "Winsock 2.2 is not supported!" << endl;
		return 0;
	}

	SOCKET client;
	client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client == INVALID_SOCKET) {
		cout << "Error: " << WSAGetLastError() << "Cannot create client socket" << endl;
		WSACleanup();
		return 0;
	}

	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)(&tv), sizeof(int));

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort);
	inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

	if (connect(client, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		cout << "Error: " << WSAGetLastError() << " Cannot connect to server" << endl;
		closesocket(client);
		WSACleanup();
		return 0;
	}


	PostArticleClient* postClient = new PostArticleClient(client);
	UserInterface uInterface(postClient);


	uInterface.displayGuide();

	uInterface.loop();

	delete postClient;
	closesocket(client);
	WSACleanup();

	return 0;
}