#include"TCPSocket.h"

class TCPConnSocket :public TCPSocket
{
private:
	int clientAddrLen;
public:
	TCPConnSocket() :TCPSocket() { clientAddrLen = sizeof(addr); };
	int* getpAddrLen() { return &clientAddrLen; }
};
