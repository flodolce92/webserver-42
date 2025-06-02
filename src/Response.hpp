#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include "header.h"

class Response
{
private:
	int					_socket;
	int					_code;
	const char*			_fileName;
	char* 				_message;
	std::string 		_header;

public:
	Response( int socket, int code, const char* fileName );
	Response( const Response& src );
	Response& operator=( const Response& src );
	~Response();
	void sendData(size_t len);
	void readFile();
};

#endif