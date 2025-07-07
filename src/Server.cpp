#include <Server.hpp>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/select.h>
#include <vector>
#include <ClientConnection.hpp>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <signal.h>
#include <sstream>

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

Server::Server(int port)
{
	// Server Config
	this->_serverPort = port;
	memset(&this->_serverAddress, 0, sizeof(this->_serverAddress));
	this->_serverAddress.sin_addr.s_addr = INADDR_ANY;
	this->_serverAddress.sin_family = AF_INET;
	this->_serverAddress.sin_port = htons(port);

	// I/O Multiplexing
	this->_maxFd = -1;
	FD_ZERO(&this->_masterReadFds);
	FD_ZERO(&this->_masterWriteFds);
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
}

Server::~Server()
{
	std::cout << "\nServer shutting down..." << std::endl;
	this->stop();

	// Clean up all clients
	std::map<int, ClientConnection *>::iterator it;

	for (it = this->_clients.begin(); it != this->_clients.end(); it++)
	{
		delete it->second;
	}

	this->_clients.clear();

	if (this->_serverFd >= 0)
	{
		close(this->_serverFd);
	}
}

// Lifecycle
bool Server::initialize()
{
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGPIPE, SIG_IGN);

	if (!this->createSocket() || !this->bindSocket() || !this->startListening())
	{
		return false;
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
	this->_timeout.tv_sec = 1; // 1 second timeout
	this->_timeout.tv_usec = 0;

	while (this->_running && !this->_shutdownRequested && !_signalReceived)
	{
		this->setupFdSets();

		int activity = select(this->_maxFd + 1, &this->_readFds, &this->_writeFds, &this->_exceptFds, &this->_timeout);
		if (activity < 0)
		{
			if (errno == EINTR)
			{
				continue; // Interrupted by signal
			}

			logError("select() failed");
			break;
		}

		if (activity == 0)
		{
			// Timed Out
			this->cleanupTimedOutClients();
			continue;
		}

		this->processFdSets(activity);
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
	// TODO: Check this
	this->_clients[clientFd] = new ClientConnection(clientFd);
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
int Server::getClientCount() const
{
	return this->_clients.size();
}
bool Server::isRunning() const
{
	return this->_running;
}
bool Server::isInitialized() const
{
	return this->_initialized;
}
int Server::getMaxFd() const
{
	return this->_maxFd;
}

void Server::setTimeout(int seconds)
{
	this->_timeout.tv_sec = seconds;
}
void Server::setBufferSize(size_t size)
{
	// TODO: Think if necessary
	(void)size;
	// this->_BUFFER_SIZE = size;
}

bool Server::setNonBlocking(int fd)
{
	// TODO: Potentially dangerous, not preserving original flags
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		logError("Failed to set non-blocking mode");
		return false;
	}
	return true;
}

bool Server::createSocket()
{
	// Create Socket
	this->_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_serverFd == -1)
	{
		logError("Failed to create server socket");
		return false;
	}

	// Set Socket Options
	if (setsockopt(this->_serverFd, SOL_SOCKET, SO_REUSEADDR,
				   &this->_REUSE_ADDR_OPT, sizeof(this->_REUSE_ADDR_OPT)) < 0)
	{
		logError("Failed to set SO_REUSEADDR");
		close(this->_serverFd);
		return false;
	}

	// Set Non-Blocking
	if (!this->setNonBlocking(this->_serverFd))
	{
		close(this->_serverFd);
		return false;
	}

	this->_maxFd = this->_serverFd;
	return true;
}

void Server::setupFdSets()
{
	// Clear All Sets
	FD_ZERO(&this->_readFds);
	FD_ZERO(&this->_writeFds);
	FD_ZERO(&this->_exceptFds);

	// Always Monitor Server Socket for New Connections
	FD_SET(this->_serverFd, &this->_readFds);
	this->_maxFd = this->_serverFd;

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

void Server::processFdSets(int activity)
{
	// Handle new connections first
	if (FD_ISSET(this->_serverFd, &this->_readFds))
	{
		this->handleNewConnection();
		activity--;
	}

	// Handle client sockets
	std::vector<int> clientFds;
	std::map<int, ClientConnection *>::const_iterator it;

	for (it = this->_clients.begin(); it != this->_clients.end(); it++)
	{
		clientFds.push_back(it->first);
	}

	for (size_t i = 0; i < clientFds.size(); i++)
	{
		int clientFd = clientFds[i];
		if (activity <= 0)
			break;

		ClientConnection *client = this->_clients[clientFd];
		if (!client)
			continue;

		// Check for exceptions first
		if (FD_ISSET(clientFd, &this->_exceptFds))
		{
			this->logError("Exception on client socket " + toString(clientFd));
			this->markClientForRemoval(clientFd);
			activity--;
			continue;
		}

		// Handle Reads
		if (FD_ISSET(clientFd, &this->_readFds))
		{
			this->handleClientRead(clientFd);
			activity--;
		}

		// Handle Writes
		if (FD_ISSET(clientFd, &this->_writeFds))
		{
			this->handleClientWrite(clientFd);
			activity--;
		}

		// Handle Client Closing
		if (client->shouldClose())
		{
			std::cout << "Detected client that needs closing: " << client->getFd() << std::endl;
			this->_clientsToRemove.push_back(client->getFd());
		}
	}
}

void Server::handleNewConnection()
{
	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);

	int clientFd = accept(this->_serverFd, (struct sockaddr *)&clientAddr, &clientAddrLen);

	if (clientFd < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			logError("Accept failed: " + std::string(strerror(errno)));
		}
		return;
	}

	// Set client socket to non blocking
	if (!this->setNonBlocking(clientFd))
	{
		close(clientFd);
		return;
	}

	// Create client connection object
	ClientConnection *client = new ClientConnection(clientFd);

	// Set Client Info
	char clientIP[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
	client->setClientInfo(clientIP, ntohs(clientAddr.sin_port));

	// Add to clients map
	this->_clients[clientFd] = client;

	std::cout << "New client connected: " << clientIP << ":" << ntohs(clientAddr.sin_port) << " (fd=" << clientFd << ")" << std::endl;
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

void Server::cleanupTimedOutClients()
{
	time_t now = time(NULL);

	// Only do cleanup every 30 seconds
	if (now - this->_lastCleanup < 30)
	{
		return;
	}

	this->_lastCleanup = now;

	std::map<int, ClientConnection *>::iterator it;

	for (it = this->_clients.begin(); it != this->_clients.end();)
	{
		ClientConnection *client = it->second;

		if (client->isTimedOut(this->_TIMEOUT_SECONDS) || client->shouldClose())
		{
			std::cout << "Removing timed out client: " << it->first << std::endl;
			delete client;
			std::map<int, ClientConnection *>::iterator temp = it;
			++it;
			this->_clients.erase(temp);
		}
		else
		{
			++it;
		}
	}
}

bool Server::bindSocket()
{
	if (bind(this->_serverFd, (struct sockaddr *)&this->_serverAddress, sizeof(this->_serverAddress)) < 0)
	{
		std::cerr << "Bind failed: " << std::string(strerror(errno)) << std::endl;
		return false;
	}

	std::cout << "Socket successfully bound to port " << this->_serverPort << "!" << std::endl;
	return true;
}

bool Server::startListening()
{
	if (listen(this->_serverFd, this->_MAX_CLIENT_CONN_QUEUE) < 0)
	{
		std::cerr << "Listen failed" << std::endl;
		return false;
	}

	std::cout << "Server is listening on port " << this->_serverPort << "!" << std::endl;
	// TODO: Rething the localhost, maybe use the IP from the config
	std::cout << "Try connecting with: curl http://localhost:" << this->_serverPort << std::endl;
	std::cout << "Press Ctrl+C to stop" << std::endl;
	return true;
}

static std::string createExampleResponse()
{
	std::string response = "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: text/html\r\n";
	response += "Connection: close\r\n";
	response += "Content-Length: 12\r\n";
	response += "\r\n";
	response += "Hello World!";

	return response;
}

void Server::handleClientRead(int clientFd)
{
	std::cout << "Handling client read for " << clientFd << std::endl;

	ClientConnection *client = this->_clients[clientFd];

	if (!client)
		return;

	client->readData();
	client->setState(CONN_PROCESSING_REQUEST);
	this->processRequest(clientFd);
}

void Server::handleClientWrite(int clientFd)
{
	std::cout << "Handling client write for " << clientFd << std::endl;

	ClientConnection *client = this->_clients[clientFd];
	client->writeData();

	if (!client)
		return;
	if (client->isKeepAlive())
		client->setState(CONN_KEEP_ALIVE);
	else
		client->setState(CONN_CLOSING);
}

void Server::removeClient(int clientFd)
{
	std::map<int, ClientConnection *>::iterator it;
	it = this->_clients.find(clientFd);

	if (it == this->_clients.end())
		return;

	std::cout << "Closing client connection: " << clientFd << std::endl;
	this->_clients.erase(it);
	delete it->second;
}

void Server::logError(const std::string &message)
{
	std::cout << "Error: " << message << std::endl;
}

void Server::handleSocketError(int clientFd, const std::string &operation)
{
	(void)clientFd;
	(void)operation;

	std::cout << "ToDO: handleSocketError" << std::endl;
}

void Server::processClientRemovalQueue()
{
	std::cout << "Processing client removal queue..." << std::endl;

	std::cout << "Clients count to remove: " << this->_clientsToRemove.size() << std::endl;
	for (size_t i = 0; i < this->_clientsToRemove.size(); ++i)
	{
		int clientFd = this->_clientsToRemove[i];

		this->removeClient(clientFd);
	}

	// Clear the queue after all removals are processed
	this->_clientsToRemove.clear();
}

void Server::shutdown()
{
	this->_shutdownRequested = true;
}

void Server::processRequest(int clientFd)
{
	ClientConnection *client = this->_clients[clientFd];

	if (!client)
		return;
	std::cout << "\n\n--------------------------" << std::endl;
	std::cout << "Client " << clientFd << "'s request: " << std::endl;
	std::cout << client->getReadBuffer() << std::endl;
	std::cout << "--------------------------" << std::endl;
	client->appendToWriteBuffer(createExampleResponse());
	client->setState(CONN_WRITING_RESPONSE);
}