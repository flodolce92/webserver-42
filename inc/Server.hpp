#ifndef SERVER_HPP
#define SERVER_HPP

# include <netinet/in.h>
# include <map>
# include <vector>

class ClientConnection;
class Buffer;

class Server
{
private:
	// Network Configuration
	int _serverFd;
	int _serverPort;
	struct sockaddr_in _serverAddress;

	// Static Configuration
	static const int _REUSE_ADDR_OPT;
	static const int _MAX_CLIENT_CONN_QUEUE;
	static const int _TIMEOUT_SECONDS;
	static const size_t _BUFFER_SIZE;

	// I/O Multiplexing
	fd_set _readFds, _writeFds;
	fd_set _masterReadFds, _masterWriteFds;
	fd_set _exceptFds;
	int _maxFd;
	
	// Client Management
	std::map<int, ClientConnection *> _clients;
	std::vector<int> _clientsToRemove;

	// Server State
	bool _running;
	bool _initialized;
	bool _shutdownRequested;

	// Timing
	struct timeval _timeout;
	time_t _lastCleanup;

	// Signal Handling
	static bool _signalReceived;
	static void signalHandler(int signal);

	// Core Socket Operations
	bool createSocket();
	bool setNonBlocking(int fd);
	bool bindSocket();
	bool startListening();

	// Connection Management
	void handleNewConnection();
	void handleClientRead(int clientFd);
	void handleClientWrite(int clientFd);
	void removeClient(int clientFd);
	void cleanupTimedOutClients();
	void processClientRemovalQueue();

	// I/O Multiplexing helpers
	void setupFdSets();
	void processFdSets(int activity);

	// Error Handling
	void handleSocketError(int clientFd, const std::string & operation);
	void logError(const std::string & message);

public:
	Server(int port);
	~Server();

	// Lifecycle
	bool initialize();
	void run();
	void stop();
	void shutdown();

	// Client Management
	bool addClient(int clientFd);
	ClientConnection *getClient(int clientFd); 
	void markClientForRemoval(int clientFd);

	// Getters
	int getClientCount() const;
	bool isRunning() const;
	bool isInitialized() const;
	int getMaxFd() const;

	// Configuration
	void setTimeout(int seconds);
	void setBufferSize(size_t size);
};

#endif
