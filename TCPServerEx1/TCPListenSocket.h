#include "TCPSocket.h"
#include "TCPConnSocket.h"


class TCPListenSocket : public TCPSocket {
public:
	TCPListenSocket() :TCPSocket() {};
	TCPListenSocket(const char* ipStr, u_short port, int backlog);
	void listenS(int backlog);
	TCPConnSocket* acceptConn();
};
