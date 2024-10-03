#include"stdfax.h"

class WinsockBuilder {
private:
	WSAData wsaData;
	WORD wVersion;
public:
	WinsockBuilder(int vers1, int vers2);

	void Startup();
	void Cleanup();
	~WinsockBuilder();
};
