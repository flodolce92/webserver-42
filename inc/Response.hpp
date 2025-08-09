#ifndef RESPONSE_HPP
# define RESPONSE_HPP

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
	int							_code;
	char* 						_message;
	std::string					_body;
	std::string 				_header;
	const Request&      		_request;
	std::map<int, std::string>	_statusCodeMessages;
	
	std::string					_host;
	std::string					_fileName;

	void 						initializeStatusCodeMessages();

public:
	Response( const ConfigManager & configManager,  const Request & request);
	Response( const Response& src );
	Response& operator=( const Response& src );
	~Response();
	void readFile();
	void setHeader(int responseCase);

	// Getters
	int							getCode() const;
	char* 						getMessage() const;
	std::string					getBody() const;
	std::string 				getHeader() const;
	std::string 				getHost() const;
	std::string 				getFileName() const;
	const Request&      		getRequest() const;
	std::map<int, std::string>	getStatusCodeMessages() const;
};

std::ostream & operator<<(std::ostream & os, const Response & response);
#endif