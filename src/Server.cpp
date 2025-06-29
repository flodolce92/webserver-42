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

// // Define static const members
// const int Server::_REUSE_ADDR_OPT = 1;
// const int Server::_MAX_CLIENT_CONN_QUEUE = 3;

bool Server::setNonBlocking(int fd) {
	// TODO: Potentially dangerous, not preserving original flags
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		logError("Failed to set non-blocking mode");
		return false;
	}
	return true;
}

bool Server::createSocket() {
	// Create Socket
	this->_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_serverFd == -1) {
		logError("Failed to create server socket");
		return false;
	}

	// Set Socket Options
	if (setsockopt(this->_serverFd, SOL_SOCKET, SO_REUSEADDR, 
		&this->_REUSE_ADDR_OPT, sizeof(this->_REUSE_ADDR_OPT)) < 0) {
			logError("Failed to set SO_REUSEADDR");
			close(this->_serverFd);
			return false;
	}

	// Set Non-Blocking
	if (!this->setNonBlocking(this->_serverFd)) {
		close(this->_serverFd);
		return false;
	}

	this->_maxFd = this->_serverFd;
	return true;
}

void Server::setupFdSets() {
	// Clear All Sets
	FD_ZERO(&this->_readFds);
	FD_ZERO(&this->_writeFds);
	FD_ZERO(&this->_exceptFds);
	// TODO: What about the master fds?

	// Always Monitor Server Socket for New Connections
	FD_SET(this->_serverFd, &this->_readFds);
	this->_maxFd = this->_serverFd;

	// Add Client Sockets
	std::map<int, ClientConnection *>::iterator it;

	for (it = this->_clients.begin(); it != this->_clients.end(); it++)
	{
		int clientFd = it->first;
		ClientConnection *client = it->second;
		
	}
}