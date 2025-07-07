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

class Response
{
private:
	int					_code;
	ClientConnection* 	_client;
	const char*			_fileName;
	char* 				_message;
	std::string			_body;
	std::string 		_header;

public:
	Response( ClientConnection *client );
	Response( const Response& src );
	Response& operator=( const Response& src );
	~Response();
	void readFile();
	void setHeader(int responseCase);
};

#endif