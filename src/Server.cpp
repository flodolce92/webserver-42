#include <Server.hpp>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/select.h>
#include <vector>

// Define static const members
const int Server::_REUSE_ADDR_OPT = 1;
const int Server::_MAX_CLIENT_CONN_QUEUE = 3;

Server::Server(int port)
{
	this->_serverFd = -1;
	this->_serverPort = port;
	this->_running = false;
	this->_maxFd = 0;
	std::cout << "Server default constructor called" << std::endl;
	memset(&this->_serverAddress, 0, sizeof(this->_serverAddress));
}

Server::~Server()
{
	std::cout << "Server destructor called" << std::endl;
	this->stop();
}

bool Server::createSocket()
{
	// Create server socket
	this->_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_serverFd == -1)
	{
		std::cerr << "Failed to create socker for the server" << std::endl;
		return false;
	}

	// Allow socket reuse
	int reuseSetup = setsockopt(this->_serverFd, SOL_SOCKET, SO_REUSEADDR, &this->_REUSE_ADDR_OPT, sizeof(this->_REUSE_ADDR_OPT));
	if (reuseSetup < 0)
	{
		std::cerr << "Allow address reuse (setsockopt) failed." << std::endl;
		return false;
	}

	this->_maxFd = this->_serverFd;
	std::cout << "Socket created successfully: " << this->_serverFd << std::endl;
	return true;
}

bool Server::bindSocket()
{
	this->_serverAddress.sin_family = AF_INET;
	this->_serverAddress.sin_addr.s_addr = INADDR_ANY;
	this->_serverAddress.sin_port = htons(this->_serverPort);

	// Bind the server socket to the IP address and port
	int bindResponse = bind(this->_serverFd, (struct sockaddr *)&this->_serverAddress, sizeof(this->_serverAddress));
	if (bindResponse < 0)
	{
		std::cerr << "Socket binding failed." << std::endl;
		return false;
	}

	std::cout << "Socket bound to port " << this->_serverPort << std::endl;
	return true;
}

bool Server::startListening()
{
	if (listen(this->_serverFd, 10) < 0)
	{
		std::cerr << "Listen failed." << std::endl;
		return false;
	}

	std::cout << "Server listening on port: " << this->_serverPort << std::endl;
	return true;
}

void Server::handleNewConnection()
{
	std::cout << "NEW CONNECTION incoming!" << std::endl;

	int newClient = accept(this->_serverFd, NULL, NULL);
	if (newClient >= 0)
	{
		this->_clientFds.push_back(newClient);
		std::cout << "Added clientFd: " << newClient << std::endl;
		std::cout << "Total clients: " << this->_clientFds.size() << std::endl;
	}
}

void Server::handleClientData(int clientFd)
{
	(void)clientFd;
}

void Server::removeClient(int clientFd)
{
	(void)clientFd;
}

bool Server::initialize()
{
	if (this->createSocket() && this->bindSocket() && this->startListening())
	{
		this->_initialized = true;
		return true;
	}
	return false;
}

void Server::run()
{
	if (!this->_initialized || this->_serverFd == -1)
	{
		std::cerr << "The server is not initialized...\nPlease initialize using the initialize() method." << std::endl;
		return;
	}
	this->_running = true;
	std::cout << "Server running on port: " << this->_serverPort << std::endl;

	while (this->_running)
	{
		// Step 1: Build fd_set
		fd_set readfds;
		FD_ZERO(&readfds);

		// Add server socket
		FD_SET(this->_serverFd, &readfds);
		this->_maxFd = this->_serverFd;

		for (size_t i = 0; i < this->_clientFds.size(); i++)
		{
			int clientFd = this->_clientFds[i];
			FD_SET(clientFd, &readfds);
			if (clientFd > this->_maxFd)
			{
				this->_maxFd = clientFd;
			}
		}

		// Step 2: Call select() - monitors ALL file descriptors
		int activity = select(this->_maxFd + 1, &readfds, NULL, NULL, NULL);

		if (activity < 0)
		{
			std::cerr << "select() failed" << std::endl;
			break;
		}

		// In case of timeout
		if (activity == 0)
		{
			continue;
		}

		std::cout << "Activity on " << activity << " descriptor(s)" << std::endl;

		// Step 3: Handle server socket (new connections)
		if (FD_ISSET(this->_serverFd, &readfds))
		{
			this->handleNewConnection();
		}

		// Step 4: Handle client sockets (incoming data)
		for (size_t i = 0; i < this->_clientFds.size(); i++)
		{
			int clientFd = this->_clientFds[i];
			// this->handleClientData(clientFd, readfds);
			if (FD_ISSET(clientFd, &readfds))
			{
				std::cout << "DATA received from clientFd: " << clientFd << std::endl;

				// Read data from this client
				char buffer[1024];
				ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer));

				if (bytesRead <= 0)
				{
					// Client disconnected
					std::cout << "Client " << clientFd << " disconnected" << std::endl;
					close(clientFd);
					this->_clientFds.erase(this->_clientFds.begin() + i);
					i--; // Adjust index after removal
				}
			}
			else
			{
				std::cout << "Sending response to client " << clientFd << std::endl;
				const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
				write(clientFd, response, strlen(response));

				// Keep connection open for more requests
				std::cout << "Response sent, keeping connection open" << std::endl;
			}
		}
	}
}

void Server::stop()
{
	close(this->_serverFd);
}

int Server::getClientCount() const
{
	return this->_clientFds.size();
}

bool Server::isRunning() const
{
	return this->_running;
}
