#include<iostream>
#include<Winsock2.h>
#include<WS2tcpip.h>


#include<fstream>
#include<string>
#include<stdlib.h>


#define ENC_CODE 0
#define DEC_CODE 1
#define TRANS_CODE 2
#define ERROR_CODE 3
#define DEFAULT_CODE 4

#define OPCODE_SIZE 1
#define LENGTH_FIELD_SIZE 4

#pragma comment(lib, "WS2_32.lib")

using namespace std;
/*
Giao dien bao gom :
+ cua so dong lenh 
+ tuong tac dong lenh 
*/
class DataHeader {
private:
	char opcode;
	
};

class Connection {
private:
	SOCKET connSock;
public:
	Connection(SOCKET s) {
		connSock = s;
	}
};

class EncDecClient {
private:
	char opcode;
	unsigned int length;
	string fileName;
public:

};

class EncDecCLI {

};

int main(int argc, char** argv) {
	
}