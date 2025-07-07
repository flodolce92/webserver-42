#include <Response.hpp>

Response::Response( ClientConnection *client ):
	_code(200),
	_client(client)
{
	std::cout << "Response class created" << std::endl;

	std::string request_str(client->getReadBuffer());
	std::string cleanRequest = request_str.substr(0, request_str.find("\n"));
	size_t pos = 0;
	std::string sub;
	std::vector<std::string> vectorRequest;
	while (pos != std::string::npos)
	{
		pos = cleanRequest.find('/');
		sub = cleanRequest.substr(0, pos);
		vectorRequest.push_back(sub);
		cleanRequest.erase(0, pos + 1);
	}
	std::vector<std::string>::const_iterator const_it = vectorRequest.begin();
	if (*const_it == "GET " and request_str[4] != ' ')
	{
		vectorRequest.erase(vectorRequest.begin());
		vectorRequest.pop_back();
		(vectorRequest.back()).erase(((vectorRequest.back()).length() - 5), 5);
		std::string filename;
		std::vector<std::string>::const_iterator i = vectorRequest.begin();
		filename = *i;
		i++;
		for ( ; i != vectorRequest.end(); i++)
		filename += '/' + *i;
		this->_fileName = filename.c_str();
		readFile();
	}
	else if (*const_it == "POST " and request_str[5] != ' ')
	{
		std::cout << "POST" << std::endl;
	}
	else if (*const_it == "DELETE " and request_str[7] != ' ')
	{
		std::cout << "DELETE" << std::endl;
	}
	else
	{
		std::cout << "not support" << std::endl;
	}
}

Response::Response( const Response& src ) :
	_code(src._code),
	_client(src._client),
	_fileName(src._fileName),
	_message(src._message),
	_header(src._header)
{
	std::cout << "Response created as a copy of Response" << std::endl;
}

Response& Response::operator=( const Response& src )
{
	std::cout << "Response assignation operator called" << std::endl;
	if (this != &src)
	{
		this->_code = src._code;
		this->_client = src._client;
		this->_fileName = src._fileName;
		this->_message = src._message;
		this->_header = src._header;
	}
	return (*this);
}

Response::~Response()
{
	std::cout << "Response class destroyed" << std::endl;
}

void Response::readFile()
{
	struct stat fileInfo;

	if (stat(this->_fileName, &fileInfo) == -1)
	{
		std::cerr << "Error getting file stats for '" << this->_fileName << "': " << strerror(errno) << std::endl;
		this->_fileName = "prova/error.html";
		this->_code = 404;
		readFile();
		return ;
	}

	int fd = open(this->_fileName, O_RDONLY);
	if (fd == -1)
	{
		std::cerr << "Error opening file '" << this->_fileName << "'. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
		this->_fileName = "prova/error.html";
		this->_code = 404;
		readFile();
		return ;
	}
	
	char* message = new char[fileInfo.st_size + 1];

	ssize_t bytes_read;
	bytes_read = read(fd, message, fileInfo.st_size);
	if (bytes_read == -1)
	{
		std::cerr << "Error reading file: " << this->_fileName << std::endl;
		delete[] message;
		close(fd);
		return ;
	}
	message[bytes_read] = '\0';
	close(fd);
	this->_body = message;
	setHeader(this->_code);
	this->_client->appendToWriteBuffer(this->_header);
	delete[] message;
}

void Response::setHeader(int responseCase)
{
	std::ostringstream oss;
	std::string contentType = "Content-Type: text/html; charset=UTF-8\r\n";
	std::string connection = "Connection: close\r\n";
	std::string server = "Server: Webserv/1.0\r\n";
	std::string contentLengthHeader;

	switch (responseCase)
	{
		case 200: // OK - Successful response
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 200 OK\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 201: // Created - Resource successfully created
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 201 Created\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 204: // No Content - Request successful, but no content to send (e.g., successful DELETE)
			this->_header = "HTTP/1.1 204 No Content\r\n" +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 400: // Bad Request - Client sent a malformed request
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 400 Bad Request\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 403: // Forbidden - Client doesn't have access rights
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 403 Forbidden\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 404: // Not Found - Resource not found
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 404 Not Found\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 405: // Method Not Allowed - HTTP method not supported for resource
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 409: // Conflict - Request could not be completed due to a conflict
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 409 Conflict\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 413: // Payload Too Large - Request entity is larger than limits defined by server
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 413 Payload Too Large\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 500: // Internal Server Error - Generic server-side error
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 500 Internal Server Error\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 501: // Not Implemented - Server does not support the functionality required to fulfill the request
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 501 Not Implemented\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		case 504: // Gateway Timeout - The server, while acting as a gateway or proxy, did not get a response from an upstream server in time
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 504 Gateway Timeout\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;

		default: // Generic default for unexpected cases or unsupported status codes
			oss.str("");
			oss << "Content-Length: " << this->_body.length() << "\r\n";
			contentLengthHeader = oss.str();
			this->_header = "HTTP/1.1 500 Internal Server Error\r\n" +
					contentType +
					contentLengthHeader +
					connection +
					server +
					"\r\n" +
					this->_body;
			break;
	}
}
