#include <Server.hpp>

int main()
{
	Server myServer = Server(8080);
	myServer.initialize();
	myServer.run();

	return 0;
}