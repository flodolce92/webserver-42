#include <Server.hpp>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/select.h>
#include <vector>
#include <ClientConnection.hpp>
#include <Response.hpp>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <signal.h>
#include <sstream>
#include <ConfigManager.hpp>
#include <Request.hpp>

// // Define static const members
const int Server::_REUSE_ADDR_OPT = 1;
const int Server::_MAX_CLIENT_CONN_QUEUE = 10;
const int Server::_TIMEOUT_SECONDS = 10;
const size_t Server::_BUFFER_SIZE = 8192;
bool Server::_signalReceived = false;

template <typename T>
static std::string toString(T value)
{
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

Server::Server(const ConfigManager &configManager) : _configManager(configManager)
{

	// I/O Multiplexing
	this->_maxFd = -1;
	FD_ZERO(&this->_readFds);
	FD_ZERO(&this->_writeFds);
	FD_ZERO(&this->_exceptFds);

	// Server State
	this->_running = false;
	this->_initialized = false;
	this->_shutdownRequested = false;

	// Timing
	this->_lastCleanup = time(NULL);
	this->_timeout.tv_sec = _TIMEOUT_SECONDS;
	this->_timeout.tv_usec = 0;

	// Connections
	this->_clients = std::map<int, ClientConnection *>();
	this->_listeningSockets = std::map<int, const ServerConfig *>();

	this->totalRequestsCount = 0;
}

Server::~Server()
{
	std::cout << "\nServer shutting down..." << std::endl;
	this->stop();

	this->_listeningSockets.clear();

	// Clean up all clients
	std::map<int, ClientConnection *>::iterator it;
	for (it = this->_clients.begin(); it != this->_clients.end(); it++)
	{
		delete it->second;
	}
	this->_clients.clear();
}

// Lifecycle
bool Server::initialize()
{
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGPIPE, SIG_IGN);

	const std::vector<ServerConfig> &serverConfigs = this->_configManager.getServers();
	if (serverConfigs.empty())
	{
		logError("No server configurations found.");
	}

	for (size_t i = 0; i < serverConfigs.size(); i++)
	{
		const ServerConfig &currentConfig = serverConfigs[i];
		int listenFd = socket(AF_INET, SOCK_STREAM, 0);
		if (listenFd < 0)
		{
			logError("Failed to create socket for " + currentConfig.host + ":" + toString(currentConfig.port) + ": " + strerror(errno));
			return false;
		}

		// Set socket options for reuse address
		if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &this->_REUSE_ADDR_OPT, sizeof(this->_REUSE_ADDR_OPT)) < 0)
		{
			logError("Failed to set SO_REUSEADDR for " + currentConfig.host + ":" + toString(currentConfig.port) + ": " + strerror(errno));
			close(listenFd);
			return false;
		}

		// Set socket to non-blocking
		if (!this->setNonBlocking(listenFd))
		{
			logError("Failed to set non-blocking for " + currentConfig.host + ":" + toString(currentConfig.port) + ": " + strerror(errno));
			close(listenFd);
			return false;
		}

		struct sockaddr_in serverAddress;
		memset(&serverAddress, 0, sizeof(serverAddress));
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(currentConfig.port);

		// Convert the localhost
		std::string host;
		if (currentConfig.host == "localhost")
			host = "127.0.0.1";
		else
			host = currentConfig.host;

		// Convert host string to network address (supports IP and localhost)
		if (inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr) <= 0)
		{
			logError("Invalid address or address not supported: " + currentConfig.host + ": " + strerror(errno));
			close(listenFd);
			return false;
		}

		// Bind the socket to the address and port
		if (bind(listenFd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
		{
			logError("Failed to bind socket to " + currentConfig.host + ":" + toString(currentConfig.port) + ": " + strerror(errno));
			close(listenFd);
			return false;
		}

		// Start listening for incoming connections
		if (listen(listenFd, this->_MAX_CLIENT_CONN_QUEUE) < 0)
		{
			logError("Failed to listen on socket for " + currentConfig.host + ":" + toString(currentConfig.port) + ": " + strerror(errno));
			close(listenFd);
			return false;
		}

		std::cout << "Server listening on http://" << currentConfig.host << ":" << toString(currentConfig.port) << std::endl;
		this->_listeningSockets[listenFd] = &currentConfig;
		FD_SET(listenFd, &this->_readFds);
		this->_maxFd = std::max(this->_maxFd, listenFd);
	}

	this->_initialized = true;
	return true;
}

void Server::run()
{
	if (!this->_initialized)
	{
		logError("Server not initialized");
		return;
	}

	this->_running = true;
	this->_timeout.tv_usec = 0;

	while (this->_running && !this->_shutdownRequested && !_signalReceived)
	{
		// Copy master fd_sets to temporary sets for select()
		this->setupFdSets();

		// Use _timeout for select, it will be modified by select() so we reset it each loop
		struct timeval currentTimeout = this->_timeout;
		int activity = select(this->_maxFd + 1, &this->_readFds, &this->_writeFds, &this->_exceptFds, &currentTimeout);

		if (_signalReceived)
		{
			break; // Exit loop on signal
		}
		if (activity < 0)
		{
			if (errno == EINTR)
			{
				continue; // Interrupted by signal
			}
			logError("select() failed: " + std::string(strerror(errno)));
			break;
		}

		if (activity == 0)
		{
			// Timed Out - Check for client timeouts
			this->cleanupTimedOutClients();
			continue;
		}

		// Process the file descriptors that have activity
		this->processFdSets();
		this->processClientRemovalQueue();
	}
}

void Server::stop()
{
	this->_running = false;
}

// Client Management
bool Server::addClient(int clientFd)
{
	// ClientConnection constructor takes an int fd
	this->_clients[clientFd] = new ClientConnection(clientFd);
	// Update maxFd if necessary
	if (clientFd > _maxFd)
	{
		_maxFd = clientFd;
	}
	return true;
}

ClientConnection *Server::getClient(int clientFd)
{
	std::map<int, ClientConnection *>::iterator it = this->_clients.find(clientFd);
	if (it == this->_clients.end())
		return NULL;
	return it->second;
}

void Server::markClientForRemoval(int clientFd)
{
	this->_clientsToRemove.push_back(clientFd);
}

// Getters
int Server::getClientCount() const { return this->_clients.size(); }
bool Server::isRunning() const { return this->_running; }
bool Server::isInitialized() const { return this->_initialized; }
int Server::getMaxFd() const { return this->_maxFd; }

void Server::setTimeout(int seconds) { this->_timeout.tv_sec = seconds; }
void Server::setBufferSize(size_t size) { (void)size; /* TODO: Think if necessary */ }

bool Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		logError("fcntl(F_GETFL) failed for fd " + toString(fd) + ": " + strerror(errno));
		return false;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		logError("fcntl(F_SETFL, O_NONBLOCK) failed for fd " + toString(fd) + ": " + strerror(errno));
		return false;
	}
	return true;
}

// Connection Management
void Server::handleNewConnection(int listenFd) // Parameter added
{
	struct sockaddr_in clientAddress;
	socklen_t clientLen = sizeof(clientAddress);
	int clientFd = accept(listenFd, (struct sockaddr *)&clientAddress, &clientLen);

	if (clientFd < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// No pending connections, just return
			return;
		}
		logError("accept() failed on listenFd " + toString(listenFd) + ": " + strerror(errno));
		return;
	}

	if (!setNonBlocking(clientFd))
	{
		logError("Failed to set client socket non-blocking for fd " + toString(clientFd));
		close(clientFd);
		return;
	}

	// Add client to master read set
	FD_SET(clientFd, &_masterReadFds);
	if (clientFd > _maxFd)
	{
		_maxFd = clientFd;
	}

	if (!addClient(clientFd))
	{
		logError("Failed to add client " + toString(clientFd));
		close(clientFd);
		FD_CLR(clientFd, &_masterReadFds);
		return;
	}

	char clientIp[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIp, INET_ADDRSTRLEN);
	int clientPort = ntohs(clientAddress.sin_port);

	_clients[clientFd]->setClientInfo(clientIp, clientPort);

	std::cout << "New connection accepted on FD " << listenFd << ", client FD: " << clientFd
			  << " from " << clientIp << ":" << clientPort << std::endl;
}

void Server::signalHandler(int signal)
{
	switch (signal)
	{
	case SIGINT:
	case SIGTERM:
		_signalReceived = true;
		break;
	case SIGPIPE:
		// Ignore SIGPIPE - we'll handle broken pipes through send/recv return values
		break;
	}
}

void Server::handleClientRead(int clientFd)
{
	ClientConnection *client = this->_clients[clientFd];
	if (!client)
		return;

	this->totalRequestsCount++;

	// Read data from the client
	if (!client->readData())
	{									// readData now returns false on EOF or real error
		markClientForRemoval(clientFd); // Client wants to close or error occurred
		return;
	}

	std::string &buffer = const_cast<std::string &>(client->getReadBuffer());

	// Process multiple requests if pipelined
	while (client->hasCompleteRequest())
	{
		// Find end of headers
		size_t headerEnd = buffer.find("\r\n\r\n");
		std::string rawRequest = buffer.substr(0, headerEnd + 4);

		// If there’s a body, extract it too
		size_t bodyLength = 0;
		size_t bodyStart = headerEnd + 4;
		if (client->hasContentLength())
		{ // you can expose a getter
			bodyLength = client->getContentLength();
		}
		if (buffer.size() < bodyStart + bodyLength)
			break;

		rawRequest = buffer.substr(0, bodyStart + bodyLength);
		buffer.erase(0, bodyStart + bodyLength); // remove processed request

		// Process the request
		this->processRequest(clientFd, rawRequest);
	}
}

void Server::handleClientWrite(int clientFd)
{
	ClientConnection *client = this->_clients[clientFd];
	if (!client)
		return;

	// Write data to the client
	if (!client->writeData())
	{									// writeData now returns false on error
		markClientForRemoval(clientFd); // Error occurred during write
		return;
	}

	// If all data is written, potentially transition state or prepare for next request
	if (!client->hasDataToWrite())
	{
		// Example: If not keep-alive, close connection, otherwise reset for next request
		if (!client->isKeepAlive())
		{
			markClientForRemoval(clientFd);
		}
		else
		{
			// Reset client for next request in keep-alive scenario
			client->setState(CONN_READING_REQUEST);
			FD_CLR(clientFd, &_masterWriteFds); // Stop monitoring for write readiness
			FD_SET(clientFd, &_masterReadFds);	// Start monitoring for read readiness
		}
	}
}

void Server::cleanupTimedOutClients()
{
	// time_t currentTime = time(NULL);
	std::vector<int> timedOutClients;

	for (std::map<int, ClientConnection *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->isTimedOut(_TIMEOUT_SECONDS) && !this->isAlreadyMarkedForRemoval(it->first))
		{
			timedOutClients.push_back(it->first);
		}
	}

	for (size_t i = 0; i < timedOutClients.size(); ++i)
	{
		std::cout << "Client " << timedOutClients[i] << " timed out." << std::endl;
		markClientForRemoval(timedOutClients[i]);
	}
}

bool Server::isAlreadyMarkedForRemoval(int clientFd)
{
	return std::find(this->_clientsToRemove.begin(), this->_clientsToRemove.end(), clientFd) != this->_clientsToRemove.end();
}

void Server::removeClient(int clientFd)
{
	std::map<int, ClientConnection *>::iterator it;
	it = this->_clients.find(clientFd);

	if (it == this->_clients.end())
		return;

	// Remove from master fd_sets
	FD_CLR(clientFd, &_masterReadFds);
	FD_CLR(clientFd, &_masterWriteFds);
	FD_CLR(clientFd, &_exceptFds); // Also clear from except set if used

	std::cout << "Closing client connection: " << clientFd << std::endl;
	delete it->second;		  // Delete the ClientConnection object
	this->_clients.erase(it); // Remove from map
	close(clientFd);		  // Close the socket FD
}

void Server::logError(const std::string &message)
{
	std::cerr << "Error: " << message << std::endl;
}

void Server::handleSocketError(int clientFd, const std::string &operation)
{
	// For now, simply log and mark for removal. More sophisticated error handling can be added.
	logError("Socket error on FD " + toString(clientFd) + " during " + operation + ": " + strerror(errno));
	markClientForRemoval(clientFd);
}

void Server::processClientRemovalQueue()
{
	if (this->_clientsToRemove.empty())
		return;

	// std::cout << "Processing client removal queue... count: " << this->_clientsToRemove.size() << std::endl;
	for (size_t i = 0; i < this->_clientsToRemove.size(); ++i)
	{
		int clientFd = this->_clientsToRemove[i];
		this->removeClient(clientFd);
	}

	// Clear the queue after all removals are processed
	this->_clientsToRemove.clear();
}

// I/O Multiplexing helpers
void Server::setupFdSets()
{
	// Clear All Sets
	FD_ZERO(&this->_readFds);
	FD_ZERO(&this->_writeFds);
	FD_ZERO(&this->_exceptFds);

	// Always Monitor Server Sockets for New Connections
	std::map<int, const ServerConfig *>::iterator sit;
	for (sit = this->_listeningSockets.begin(); sit != this->_listeningSockets.end(); sit++)
	{
		int serverFd = sit->first;
		FD_SET(serverFd, &this->_readFds);
		this->_maxFd = serverFd;
	}

	// Add Client Sockets
	std::map<int, ClientConnection *>::iterator it;

	// Add client sockets
	for (it = this->_clients.begin(); it != this->_clients.end(); it++)
	{
		int clientFd = it->first;
		ClientConnection *client = it->second;

		if (client->needsRead())
			FD_SET(clientFd, &this->_readFds);

		if (client->needsWrite())
			FD_SET(clientFd, &this->_writeFds);

		// Always monitor for exceptions
		FD_SET(clientFd, &this->_exceptFds);

		if (clientFd > this->_maxFd)
			this->_maxFd = clientFd;
	}
}

void Server::processFdSets()
{
	for (int fd = 0; fd <= _maxFd; ++fd)
	{
		// Check for activity on listening sockets
		if (FD_ISSET(fd, &_readFds))
		{
			if (_listeningSockets.count(fd)) // Is it a listening socket?
			{
				handleNewConnection(fd);
				continue; // Move to next FD
			}
		}

		// Check for activity on client sockets
		// Ensure it's a known client AND it's not already marked for removal
		if (_clients.count(fd))
		{
			// Check if client needs to be read from
			if (FD_ISSET(fd, &_readFds) && _clients[fd]->needsRead())
			{
				handleClientRead(fd);
			}
			// Check if client needs to be written to
			if (FD_ISSET(fd, &_writeFds) && _clients[fd]->needsWrite())
			{
				handleClientWrite(fd);
			}
		}
	}
}

void Server::shutdown()
{
	this->_shutdownRequested = true;
}

void Server::processRequest(int clientFd, const std::string &rawRequest)
{
	ClientConnection *client = this->_clients[clientFd];
	if (!client)
		return;

	// Get current time for logging
	time_t now = time(NULL);
	char timeBuffer[80];
	struct tm *timeInfo = localtime(&now);
	strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeInfo);

	// std::cout << "\n\n--------------------------" << std::endl;
	// std::cout << "Current time: " << timeBuffer << std::endl;
	// std::cout << "[" << this->totalRequestsCount << "] Client " << clientFd << "'s request:\n";
	// std::cout << rawRequest << std::endl;
	// std::cout << "--------------------------" << std::endl;

	Request request(rawRequest);
	std::cout << request.toString() << std::endl;

	// Keep-Alive handling
	if (!request.getHeaderValues("connection").empty())
	{
		client->setKeepAlive(
			request.getHeaderValues("connection")[0].find("keep-alive") != std::string::npos);
	}

	client->setState(CONN_WRITING_RESPONSE);
	Response response(this->_configManager, request);
	client->appendToWriteBuffer(response.get());
	this->handleClientWrite(client->getFd());
}
