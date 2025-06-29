#ifndef SERVER_HPP
#define SERVER_HPP

# include <netinet/in.h>
# include <vector>

class Server
{
private:
	int _serverFd;
	int _serverPort;
	struct sockaddr_in _serverAddress;

	
	static const int _REUSE_ADDR_OPT;
	static const int _MAX_CLIENT_CONN_QUEUE;
	
	std::vector<int> _clientFds;
	int _maxFd;
	bool _running;
	bool _initialized;

	// Helper functions
	bool createSocket();
	bool bindSocket();
	bool startListening();
	void handleNewConnection();
	void handleClientData(int clientFd);
	void removeClient(int clientFd);

public:
	Server(int port);
	~Server();

	// Lifecycle
	bool initialize();
	void run();
	void stop();

	// Getters for debugging
	int getClientCount() const;
	bool isRunning() const;
};

#endif