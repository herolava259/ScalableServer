#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include<Winsock2.h>
#include<Windows.h>
#include<WS2tcpip.h>
#include<WinUser.h>

#include<sstream>
#include<fstream>
#include<string>

#include<stdlib.h>
#include<time.h>

#include<stdio.h>

#pragma comment(lib,"Ws2_32.lib")

#define WM_SOCKET WM_USER +1
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define MAX_CLIENT 1024
#define BUFF_SIZE 2048

#define ENC_CODE 0
#define DEC_CODE 1
#define TRANS_CODE 2
#define ERROR_CODE 3

#define DEFAULT_CODE 4

#define LENGTH_NAME 7


#define INIT_TYPE -1
#define ENC_TYPE 0
#define DEC_TYPE 1

#define INIT_STATUS -1
#define RECV_DATA_STATUS 0
#define SEND_DATA_STATUS 1
#define FINISH_STATUS 2
#define ERROR_STATUS 3
#define TCP_ERROR_STATUS 4

#define OPCODE_SIZE 1
#define LENGTH_FIELD_SIZE 4

#define LIMIT_INTERGER 0xffffffff
#define LIMIT_TRY_NUM 10

using namespace std;
ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);

LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);

SOCKET client[MAX_CLIENT];
SOCKET listenSock;
int num_client = 0;

const char* clsName = "Encode-Decode File Server";
const char* wndName = "Encode-Decode File Window";
class DataPackage {
private:
	byte opcode;
	unsigned int length;
	string filePath;
public:

	static string createRandomName() {
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
	DataPackage(byte code = DEFAULT_CODE, int length = 0, string filePath = "") {
		opcode = DEFAULT_CODE;
		this->length = length;
		this->filePath = filePath;

		if (this->filePath.size() == 0) {
			this->filePath = createRandomName();
		}
	}

	bool setOpcode(byte code){
		if (code >= 0 && code <= 3) {
			opcode = code;
			return true;
		}
		else {
			return false;
		}
		return true;
	}

	void setLength(u_int length) {
		this->length = length;
	}

	bool operator<< (const byte* data) {


		return true;
	}
};

// Thuc hien 1 phien enc/dec tren 1 client 
class EncDecSession {
private:
	// ten file tam chua du lieu thuc hien
	string fileName;

	// trang thai cua qua trinh thuc hien 
	int status;

	// ma hoa hay giai ma
	int type;

	// khoa ma hoa hoac giai ma
	byte key;

	// socket tuong ung voi client dang phuc vu
	SOCKET connSock;

	// kich thuoc tinh theo byte cua file thuc hien
	unsigned long length;

	// Ham tao ten file tam ngau nhien
	string createRandomName(unsigned int rnd_seed) {
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

	// chuyen mang char 4 phan tu thanh 1 so nguyen khong dau 
	unsigned int byteArr_to_interger(char* first) {
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

	// chuyen so nguyen thanh mang 4 byte 
	void interger_to_byteArr(unsigned int num, char * buff) {
		*(buff+3) = (char)num;
		*(buff + 2) = (char)(num >> 8);
		*(buff + 1) = (char)(num >> 16);
		*buff = (char)(num >> 24);
	}

	//ma hoa va giai ma tuy theo kieu type
	char encode(char letter) {
		if (type == ENC_TYPE) {
			return (char)((letter + (int)key) % 256);
		}
		else if(type == DEC_TYPE){
			return (char)((letter - (int)key) % 256);
		}
		return 0;
	}

	// ma hoa hay giai ma roi chuyen du lieu vao file tam
	void encrypt_and_write_data_to_file(fstream& stream, char* dat, int length) {

		for (int i = 0; i < length; i++) {
			stream.put(encode(*(dat+i)));
			if (0) {
				status = ERROR_STATUS;
			}
		}
	}
	
	int recvTCP(char * buff, int buffSize = BUFF_SIZE) {
		int ret = -1;
		ret = recv(connSock, buff, buffSize, BUFF_SIZE);

		if (ret == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}
		else if (ret == 0) {
			return 0;
		}
		else {
			buff[ret] = 0;
			return ret;
		}
	}

	int sendTCP(char* buff, int length) {
		int ret = 0;
		ret = send(connSock, buff, length, 0);
		if (ret <= 0) {
			return 0;
		}
		else {
			return 1;
		}
	}

    // Noi du lieu tu mang src vao cuoi mang dst
	int concatData(char* dst, int lenDst, char* src, int lenSrc) {
		for (int i = lenDst; i < lenDst + lenSrc; i++) {
			dst[i] = src[i - lenDst];
		}
		dst[lenDst + lenSrc] = 0;
		return lenDst + lenSrc;
	}

	// Dich cac byte tu vi tri shift_idx den cuoi arr len dau 
	int shift_bytes_to_first(char* arr, int len, int shift_idx) {
		int i = 0;
		for (int i = 0; i < len - shift_idx ; i++) {
			arr[i] = arr[i + shift_idx];

		}
		return i;
	}

	//Doc du lieu tu file vao mang byte buff
	int read_data_from_file(fstream& stream, char* buff, int length) {
		char c = 0;
		int i = 0;
		while (stream.get(c)&& i < length) {
			buff[i] = c;
			i++;
			if (0) {
				status = ERROR_STATUS;
			}
		}
		return i;
	}
public:
	EncDecSession(SOCKET connSock,unsigned int rnd_seed) {
		this->connSock = connSock;
		fileName = createRandomName((unsigned int)time(0)+rnd_seed);
		status = INIT_STATUS;
		type = INIT_TYPE;
		key = 0;
	}

	// tra ve trang thai hien tai trong qua trinh truyen file
	int getStatus() {
		return status;
	}

	// tra lai kieu dich vu: ma hoa hay giai ma
	int getType() {
		return type;
	}
	
	// bat dau phien bang cach nhan 1 goi tin man co dich vu va khoa
	void beginSession() {
		if (status != INIT_STATUS) {
			return;
		}
		int ret;
		char buff[BUFF_SIZE];
		ret = recvTCP(buff, BUFF_SIZE);
		if (ret <=0) {
			status = TCP_ERROR_STATUS;
			return;
		}
		byte opcode = buff[0];
		if (opcode == ENC_CODE) {
			type = ENC_TYPE;
			status = RECV_DATA_STATUS;
		}
		else if(opcode == DEC_CODE) {
			type = DEC_TYPE;
			status = RECV_DATA_STATUS;
		}
		else {
			status = ERROR_STATUS;
			return;
		}

		if (OPCODE_SIZE + LENGTH_FIELD_SIZE <= ret) {
			key = buff[OPCODE_SIZE + LENGTH_FIELD_SIZE];
		}
		else {
			status = ERROR_STATUS;
		}
	}

	// thuc hien khoi dong lai phien : trang thai, ten file , xoa file tam tao ra da ton tai, status, type, length
	void restartSession() {
		status = INIT_STATUS;
		type = INIT_TYPE;
		key = 0;
		if (fileName.size() != 0) {
			int sts = remove(fileName.c_str());
			if (sts != 0) {
				status = ERROR_STATUS;
			}
		}
		fileName = createRandomName((unsigned int)time(0));
		length = 0;
	}

	// nhan file tu phia client
	// dam bao duoc:
	// + 
	void recvFile() {
		
		if (status != RECV_DATA_STATUS || type == INIT_TYPE) {
			return;
		}
		fstream tmpStream;
		char buff[BUFF_SIZE];
		char dataPkg[BUFF_SIZE+BUFF_SIZE];// Luu du lieu con lai 
		dataPkg[0] = 0;
		int ret,  dataPkg_n =0;
		tmpStream.open(fileName, ios::out | ios::trunc| ios::binary);

		// moi vong lap la 1 lan doc 1 thong diep hoan chinh 
		while (1) {
			unsigned int len = 0,buffSize = BUFF_SIZE ;
			ret = recvTCP(buff, BUFF_SIZE);
			if (ret <= 0) {
				status = TCP_ERROR_STATUS;
				tmpStream.close();
				return;
			}
			dataPkg_n = concatData(dataPkg, dataPkg_n, buff, ret);
			byte code = dataPkg[0];
			if (code != TRANS_CODE) {
				status = ERROR_STATUS;
				tmpStream.close();
				return;
			}
			len = byteArr_to_interger(dataPkg + 1);
			if (len == 0) {
				status = SEND_DATA_STATUS;
				tmpStream.close();
				return;
			}
			length += len;
			unsigned int numRcvdByte = dataPkg_n - (OPCODE_SIZE + LENGTH_FIELD_SIZE);
			if (len <= numRcvdByte) {
				encrypt_and_write_data_to_file(tmpStream, dataPkg + (OPCODE_SIZE + LENGTH_FIELD_SIZE), len);
				if (status == ERROR_STATUS) {
					tmpStream.close();
					return;
				}
				dataPkg_n = shift_bytes_to_first(dataPkg, dataPkg_n, OPCODE_SIZE + LENGTH_FIELD_SIZE + len);
				continue;
			}
			else {
				encrypt_and_write_data_to_file(tmpStream, dataPkg + (OPCODE_SIZE + LENGTH_FIELD_SIZE), numRcvdByte);
				if (status == ERROR_STATUS) {
					tmpStream.close();
					return;
				}
				dataPkg_n = 0;
			}


			while (numRcvdByte < len ) {
				if (numRcvdByte + buffSize > len) {
					buffSize = len - numRcvdByte;
				}
				ret = recvTCP(buff, buffSize);
				if (ret <= 0) {
					status = TCP_ERROR_STATUS;
					tmpStream.close();
					return;
				}
				encrypt_and_write_data_to_file(tmpStream, buff, ret);
				if(status == ERROR_STATUS){
					tmpStream.close();
					return;
				}
				numRcvdByte += ret; 
			}
		}
		tmpStream.close();
	}
	void sendFile() {
		if (status != SEND_DATA_STATUS || type == INIT_TYPE) {
			return;
		}
		fstream readStream;
		char buff[BUFF_SIZE];
		unsigned int buffSize = BUFF_SIZE;
		unsigned long remainBytes = length;
		int ret = 0;

		readStream.open(fileName, ios::in| ios::binary);
		// GUi file dau  tien
		while (remainBytes > 0) {
			unsigned int len = 0;
			buff[0] = TRANS_CODE;
			if (remainBytes+OPCODE_SIZE+LENGTH_FIELD_SIZE < BUFF_SIZE) {
				buffSize = remainBytes + OPCODE_SIZE + LENGTH_FIELD_SIZE;
				interger_to_byteArr(remainBytes, buff + OPCODE_SIZE);
				len = remainBytes;

			}
			else {
				buffSize = BUFF_SIZE;
				if (remainBytes <= LIMIT_INTERGER) {
					interger_to_byteArr(remainBytes, buff + OPCODE_SIZE);
					len = remainBytes;
				}
				else {
					interger_to_byteArr(LIMIT_INTERGER, buff + OPCODE_SIZE);
					len = LIMIT_INTERGER;
				}
			}
			remainBytes -= (unsigned long)len;
			buffSize = read_data_from_file(readStream, buff + OPCODE_SIZE + LENGTH_FIELD_SIZE, buffSize - (OPCODE_SIZE + LENGTH_FIELD_SIZE));
			ret = sendTCP(buff, buffSize + (OPCODE_SIZE + LENGTH_FIELD_SIZE));
			if (ret <= 0) {
				status = TCP_ERROR_STATUS;
				readStream.close();
				return;
			}
			len -= buffSize - (OPCODE_SIZE + LENGTH_FIELD_SIZE);
			while (len > 0 && buffSize > 0) {
				buffSize = BUFF_SIZE;
				buffSize = read_data_from_file(readStream, buff, buffSize);
				ret = sendTCP(buff, buffSize);
				if (ret <= 0) {
					status = TCP_ERROR_STATUS;
					readStream.close();
					return;
				}
				len -= buffSize;
			}
			
		}
		buff[0] = TRANS_CODE;
		interger_to_byteArr(0, buff + OPCODE_SIZE);
		ret = sendTCP(buff, OPCODE_SIZE + LENGTH_FIELD_SIZE);
		if (ret <= 0) {
			status = TCP_ERROR_STATUS;
			readStream.close();
			return;
		}
		status = FINISH_STATUS;
		readStream.close();
	}
	void sendError() {
		if (status != ERROR_STATUS) {
			return;
		}
		char buff[BUFF_SIZE];
		buff[0] = ERROR_CODE;
		interger_to_byteArr(0, buff + OPCODE_SIZE);
		int ret = 0;
		ret = sendTCP(buff, OPCODE_SIZE + LENGTH_FIELD_SIZE);

		if (ret <= 0) {
			status = TCP_ERROR_STATUS;
		}
	}
};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	MSG msg;
	HWND serverWindow;

	if (!MyRegisterClass(hInstance))
	{
		MessageBox(NULL, "Cannot register winodw", "Error", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	if ((serverWindow = InitInstance(hInstance, nCmdShow)) == NULL)
	{
		return FALSE;
	}

	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);

	if (WSAStartup(wVersion, &wsaData)) {
		MessageBox(serverWindow, "Winsock 2.2 isnot supported.", "Error!", MB_OK);
		return 0;
	}
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	WSAAsyncSelect(listenSock, serverWindow, WM_SOCKET, FD_ACCEPT | FD_CLOSE | FD_READ);
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		MessageBox(serverWindow, "Cannot place serversocket in state LISTEN.", "Error!", MB_OK);

	}

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = windowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = clsName;
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	return RegisterClassEx(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow) {
	HWND hWnd;
	int i;
	for (i = 0; i < MAX_CLIENT; i++) {
		client[i] = 0;
	}

	hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, clsName, wndName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		return FALSE;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

LRESULT CALLBACK windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	SOCKET connSock;
	sockaddr_in clientAddr;

	int clientAddrLen = sizeof(clientAddr), i;

	

	switch (message) {
	case WM_SOCKET:
	{
		if (WSAGETSELECTERROR(lParam)) {
			for (i = 0; i < MAX_CLIENT; i++) {
				if (client[i] == (SOCKET)wParam) {
					closesocket(client[i]);
					client[i] = 0;
					continue;
				}
			}
		}

		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_ACCEPT: 
		{
			connSock = accept((SOCKET)wParam, (sockaddr*)&clientAddr, &clientAddrLen);
			if (connSock == INVALID_SOCKET) {
				break;
			}
			for (i = 0; i < MAX_CLIENT; i++) {
				if (client[i] == 0) {
					client[i] = connSock;
					num_client++;
					WSAAsyncSelect(client[i], hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
					break;
				}
			}
			if (i == MAX_CLIENT) {
				MessageBox(hWnd, "Too many Clients!", "Notice", MB_OK);
			}
			break;
		}

		case FD_READ:
		{
			for (i = 0; i < MAX_CLIENT; i++) {
				if (client[i] == (SOCKET)wParam)
				{
					EncDecSession* pSess = new EncDecSession(client[i], i);
					int try_num = 0;
					
					while (pSess->getStatus() != FINISH_STATUS && try_num < LIMIT_TRY_NUM) {
						pSess->beginSession();
						pSess->recvFile();
						pSess->sendFile();

						if (pSess->getStatus() == TCP_ERROR_STATUS) {
							closesocket(client[i]);
							client[i] = 0;
							break;
						}
						else if (pSess->getStatus() == ERROR_STATUS) {
							pSess->sendError();
							pSess->restartSession();
						}
						try_num++;
					}
					delete pSess;
				}
			}
			
			break;
		}

		case FD_CLOSE:
		{
			for(i =0; i < MAX_CLIENT; i++)
				if (client[i] == (SOCKET)wParam) {
					closesocket(client[i]);
					client[i] = 0;
					break;
				}
			break;
		}

		}
		break;
	}

	case WM_DESTROY:
	{
		PostQuitMessage(0);
		shutdown(listenSock, SD_BOTH);
		closesocket(listenSock);
		WSACleanup();
		return 0;
		break;
	}

	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
		shutdown(listenSock, SD_BOTH);
		closesocket(listenSock);
		WSACleanup();
		return 0;
		break;
	}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

