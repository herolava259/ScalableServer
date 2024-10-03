#include"TCPListenSocket.h"

TCPListenSocket::TCPListenSocket(const char* ipStr, u_short port, int backlog) :TCPSocket() {
	this->setPort(port)->setIPAddr(ipStr)->bind();
	listenS(backlog);

}

void TCPListenSocket::listenS(int backlog) {
	if (listen(s, backlog)) {
		throw "Cannot listen on the socket";
	}
}

TCPConnSocket* TCPListenSocket::acceptConn() {
	TCPConnSocket* pConnSock = new TCPConnSocket();
	sockaddr_in* addr = &(pConnSock->getAddr());

	SOCKET sock;
	sock = accept(this->s, (sockaddr*)addr, pConnSock->getpAddrLen());
	if (sock == SOCKET_ERROR) {
		delete pConnSock;
		pConnSock = NULL;
		throw "Cannot incomming connection !";
	}
	else {
		pConnSock->setSocket(sock);
	}
	return pConnSock;
}
