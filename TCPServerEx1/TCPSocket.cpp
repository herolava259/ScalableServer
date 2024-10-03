#include"TCPSocket.h"
#include"stdfax.h"

TCPSocket::TCPSocket() {
	addr.sin_family = AF_INET;
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {

		throw "Cannot create socket";
	}
}
void TCPSocket::setSocket(SOCKET sock) {
	closesocket(s);
	s = sock;
}


TCPSocket* TCPSocket::setIPAddr(const char* addr) {

	inet_pton(AF_INET, addr, &(this->addr).sin_addr);
	return this;
}


char* TCPSocket::getIPStr() {
	char ipStr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));

	return ipStr;
}

TCPSocket* TCPSocket::setPort(u_short port) {
	addr.sin_port = htons(port);
	return this;
}
u_short TCPSocket::getPort() {
	return ntohs(this->addr.sin_port);
}

TCPSocket::~TCPSocket() {
	closeSocket();
}
