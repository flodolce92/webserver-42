#include <Server.hpp>

int main(void)
{
	Server sv = Server(8080);
	sv.initialize();
	sv.run();

	return 0;
}