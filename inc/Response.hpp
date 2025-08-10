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
	std::string _body;
	std::string _fullResponse;
	StatusCodes::StatusCode _status;
	const Request &_request;
	std::map<int, std::string> _statusCodeMessages;
	const ServerConfig *_serverConfig;
	const Location *_matchedLocation;
	std::string _method;

	std::string _fileContent;
	std::string _host;
	int _port;
	std::string _fileName;
	const ConfigManager &_configManager;

	void handleGet();
	void handlePost();
	void handleDelete();
	void handleUnsupported();

	// Helpers
	std::string _errorPageFilePath;

public:
	Response(const ConfigManager &configManager, const Request &request);
	Response(const Response &src);
	Response &operator=(const Response &src);
	~Response();
	void readFile();
	void buildResponse();

	std::string get() const;
};

std::ostream &operator<<(std::ostream &os, const Response &response);
#endif