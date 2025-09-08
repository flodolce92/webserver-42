#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

class ClientConnection;
class Buffer;
class ConfigManager;
struct ServerConfig; // Forward declare ServerConfig

class Server
{
private:
	// Server Config
	const ConfigManager &_configManager;
	std::map<int, const ServerConfig *> _listeningSockets; // Map listening FDs to their configs

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

	// Connection Management
	void handleNewConnection(int listenFd); // Modified signature
	void handleClientRead(int clientFd);
	void handleClientWrite(int clientFd);
	void removeClient(int clientFd);
	void cleanupTimedOutClients();
	void processClientRemovalQueue();
	void processRequest(int clientFd, const std::string &rawRequest);
	bool isAlreadyMarkedForRemoval(int clientFd);

	// I/O Multiplexing helpers
	void setupFdSets();
	void processFdSets(); // Modified signature

	// Error Handling
	void handleSocketError(int clientFd, const std::string &operation);
	void logError(const std::string &message);

	int totalRequestsCount;

public:
	Server(const ConfigManager &configManager);
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
	void setTimeout(int seconds);
	void setBufferSize(size_t size);
	bool setNonBlocking(int fd); // Moved to public, as it's a utility
};

#endif