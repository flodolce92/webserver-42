#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

#include <fstream>
#include <iostream>
#include <sstream>

#include <ClientConnection.hpp>
#include <ConfigManager.hpp>
#include <FileServer.hpp>
#include <CGIHandler.hpp>
#include <Request.hpp>
#include <map>
#include <StatusCodes.hpp>

class Response
{
private:
	const Request &_request;

	// Response parts
	std::string _body;
	StatusCodes::Code _status;
	std::string mimeType;

	// Full response (headers + body)
	std::string _fullResponse;

	// Server Config
	std::string _host;
	int _port;
	const ConfigManager &_configManager;
	const ServerConfig *_server;

	// Method Handlers
	void handleGet();
	void handlePost();
	void handleDelete();
	void handleUnsupported();

	// Initializers
	int initPortAndHost();

	// Helpers
	std::string _errorPageFilePath;
	std::string _filePath;
	std::map<int, std::string> _statusCodeMessages;
	const Location *_matchedLocation;
	bool hasError() const;
	void setErrorFilePathForStatus(StatusCodes::Code status);

	// Getters
	std::string getMethod() const;
	std::string getFileName() const;

	// Response Builders
	void buildResponseContent();
	void readFile();

public:
	Response(const ConfigManager &configManager, const Request &request);
	Response(const Response &src);
	Response &operator=(const Response &src);
	~Response();

	// Returns the properly formatted response as a string
	std::string get() const;
};

#endif