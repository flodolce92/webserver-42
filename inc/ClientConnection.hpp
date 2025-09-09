#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include <string>

enum ConnectionState
{
	CONN_READING_REQUEST,
	CONN_PROCESSING_REQUEST,
	CONN_WRITING_RESPONSE,
	CONN_KEEP_ALIVE,
	CONN_CLOSING,
	CONN_ERROR
};

class ClientConnection
{
private:
	int _fd;
	ConnectionState _state;
	time_t _lastActivity;
	time_t _createdAt;

	// Buffers
	std::string _readBuffer;
	std::string _writeBuffer;
	size_t _writeOffset;

	// HTTP Parsing State
	size_t _contentLength;
	bool _hasContentLength;
	// bool _isChunked;

	// Connection Properties
	bool _keepAlive;
	std::string _clientIP;
	int _clientPort;

	// Statistics
	size_t _bytesRead;
	size_t _bytesWritten;
	int _requestCount;

public:
	ClientConnection(int fd);
	~ClientConnection();

	// I/O Operations
	bool readData();
	bool writeData();
	void appendToWriteBuffer(const std::string &data);
	void clearReadBuffer();
	void clearWriteBuffer();

	// State Management
	ConnectionState getState() const;
	void setState(ConnectionState state);
	void updateActivity();

	// Buffer Access
	const std::string &getReadBuffer() const;
	const std::string &getWriteBuffer() const;
	bool hasDataToWrite() const;
	bool hasCompleteRequest() const;

	// Timeout Checking
	bool isTimedOut(time_t timeout) const;
	time_t getLastActivity() const;
	time_t getAge() const;

	// Connection Properties
	int getFd() const;
	bool isKeepAlive() const;
	void setKeepAlive(bool keepAlive);

	// Client Info
	void setClientInfo(const std::string &ip, int port);
	const std::string &getClientIP() const;
	int getClientPort() const;

	// Statistics
	size_t getBytesRead() const;
	size_t getBytesWritten() const;
	int getRequestCount() const;
	void incrementRequestCount();
	size_t getContentLength() const;

	// Setters
	void setContentLength(int cl);

	// Helper Methods for Server Class
	bool needsRead() const;
	bool needsWrite() const;
	bool shouldClose() const;
	bool hasContentLength() const;
};

#endif