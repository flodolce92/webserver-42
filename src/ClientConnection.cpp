#include <ClientConnection.hpp>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cmath>

static time_t getCurrentTime()
{
	time_t now = time(NULL);
	return now;
}

ClientConnection::ClientConnection(int fd)
	: _fd(fd),
	  _state(CONN_READING_REQUEST),
	  _writeOffset(0),
	  _contentLength(0),
	  _hasContentLength(false),
	  //   _isChunked(false),
	  _keepAlive(false),
	  _clientPort(0),
	  _bytesRead(0),
	  _bytesWritten(0),
	  _requestCount(0)
{
	// Set creation time and last activity to current time
	time_t now = getCurrentTime();
	this->_createdAt = now;
	this->_lastActivity = now;
}

ClientConnection::~ClientConnection()
{
	// Close socket if it's still open
	if (this->_fd != -1)
	{
		close(this->_fd);
		this->_fd = -1;
	}
}

// I/O Operations
bool ClientConnection::readData()
{
	char buffer[4096];
	ssize_t bytesRead = recv(this->_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);

	if (bytesRead < 0)
	{
		this->setState(CONN_ERROR);
		return false;
	}

	if (bytesRead == 0)
	{
		// Client Closed Connection
		this->setState(CONN_CLOSING);
		return false;
	}

	// Client Requested Resources
	buffer[bytesRead] = '\0';
	this->_readBuffer.append(buffer, bytesRead);
	this->_bytesRead += bytesRead;
	this->updateActivity();
	return true;
}

bool ClientConnection::writeData()
{
	if (!this->hasDataToWrite())
	{
		return true;
	}

	const char *data = this->_writeBuffer.c_str() + this->_writeOffset;
	size_t remaining = this->_writeBuffer.size() - this->_writeOffset;

	ssize_t bytesWritten = send(this->_fd, data, remaining, MSG_DONTWAIT);

	if (bytesWritten < 0)
	{
		// Real Error
		this->setState(CONN_ERROR);
		return false;
	}

	if (bytesWritten == 0)
	{
		// Should Not Happen with send()
		return true;
	}

	this->_writeOffset += bytesWritten;
	this->_bytesWritten += bytesWritten;
	this->updateActivity();

	// Check if All Data Written
	if (this->_writeOffset >= this->_writeBuffer.size())
	{
		this->clearWriteBuffer();
		if (this->_state == CONN_WRITING_RESPONSE)
		{
			setState(this->_keepAlive ? CONN_KEEP_ALIVE : CONN_CLOSING);
		}
	}
	return true;
}

void ClientConnection::appendToWriteBuffer(const std::string &data)
{
	if (this->_writeBuffer.empty())
		this->_writeOffset = 0;
	this->_writeBuffer.append(data);
}

void ClientConnection::clearReadBuffer()
{
	this->_readBuffer.clear();
}

void ClientConnection::clearWriteBuffer()
{
	this->_writeOffset = 0;
	this->_writeBuffer.clear();
}

// State Management
ConnectionState ClientConnection::getState() const
{
	return this->_state;
}

void ClientConnection::setState(ConnectionState state)
{
	this->_state = state;
}

void ClientConnection::updateActivity()
{
	this->_lastActivity = getCurrentTime();
}

// Buffer Access
const std::string &ClientConnection::getReadBuffer() const
{
	return this->_readBuffer;
}

const std::string &ClientConnection::getWriteBuffer() const
{
	return this->_writeBuffer;
}

bool ClientConnection::hasDataToWrite() const
{
	return !_writeBuffer.empty() && _writeOffset < _writeBuffer.size();
}

bool ClientConnection::hasCompleteRequest() const
{
	size_t headerEnd = this->_readBuffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false; // no full headers yet

	// Parse headers quickly to find Content-Length (simplified)
	size_t contentLength = 0;
	size_t pos = this->_readBuffer.find("Content-Length:");
	if (pos != std::string::npos)
	{
		size_t end = this->_readBuffer.find("\r\n", pos);
		std::string lenStr = this->_readBuffer.substr(pos + 15, end - (pos + 15));
		contentLength = atoi(lenStr.c_str());

		// Update the instance variables for use by other methods
		const_cast<ClientConnection *>(this)->_contentLength = contentLength;
		const_cast<ClientConnection *>(this)->_hasContentLength = true;
	}

	size_t totalLength = headerEnd + 4 + contentLength;
	return this->_readBuffer.size() >= totalLength;
}

// Timeout Checking
bool ClientConnection::isTimedOut(time_t timeout) const
{
	return getCurrentTime() - this->getLastActivity() > timeout;
}

time_t ClientConnection::getLastActivity() const
{
	return this->_lastActivity;
}

time_t ClientConnection::getAge() const
{
	return this->_createdAt;
}

// Connection Properties
int ClientConnection::getFd() const
{
	return this->_fd;
}

bool ClientConnection::isKeepAlive() const
{
	return this->_keepAlive;
}

void ClientConnection::setKeepAlive(bool keepAlive)
{
	this->_keepAlive = keepAlive;
}

// Client Info
void ClientConnection::setClientInfo(const std::string &ip, int port)
{
	this->_clientIP = ip;
	this->_clientPort = port;
}

const std::string &ClientConnection::getClientIP() const
{
	return this->_clientIP;
}

int ClientConnection::getClientPort() const
{
	return this->_clientPort;
}

size_t ClientConnection::getContentLength() const
{
	return this->_contentLength;
}

bool ClientConnection::hasContentLength() const
{
	return this->_hasContentLength;
}

// Statistics
size_t ClientConnection::getBytesRead() const
{
	return this->_bytesRead;
}

size_t ClientConnection::getBytesWritten() const
{
	return this->_bytesWritten;
}

int ClientConnection::getRequestCount() const
{
	return this->_requestCount;
}

void ClientConnection::incrementRequestCount()
{
	this->_requestCount++;
}

// Helper Methods for Server Class
bool ClientConnection::needsRead() const
{
	return this->_state == CONN_READING_REQUEST;
}

bool ClientConnection::needsWrite() const
{
	return this->_state == CONN_WRITING_RESPONSE;
}

bool ClientConnection::shouldClose() const
{
	return this->_state == CONN_CLOSING;
}

void ClientConnection::setContentLength(int cl)
{
	this->_contentLength = cl;
}