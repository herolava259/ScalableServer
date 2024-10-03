#include<Winsock2.h>
#include<WS2tcpip.h>

class TCPSocket {
protected:
	SOCKET s;
	sockaddr_in addr;

public:
	TCPSocket();

	SOCKET getSocket() { return this->s; }
	void setSocket(SOCKET sock);

	sockaddr_in getAddr() { return this->addr; }
	char* getIPStr();

	TCPSocket* setPort(u_short port);
	u_short getPort();

	TCPSocket* setIPAddr(const char* addr);

	void bind();

	void closeSocket() { closesocket(s); };

	~TCPSocket();
};
