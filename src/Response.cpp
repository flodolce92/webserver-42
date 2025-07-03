#include "Response.hpp"

Response::Response( int socket, int code, const char* fileName) :
	_socket(socket),
	_code(code),
	_fileName(fileName)
{
	std::cout << "Response class created" << std::endl;
}

Response::Response( const Response& src ) :
	_socket(src._socket),
	_code(src._code),
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
		this->_socket = src._socket;
		this->_code = src._code;
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

void Response::sendData(size_t len)
{
	long bytes_sent = write(this->_socket, this->_header.c_str(), len);
	if (bytes_sent < 0)
		std::cerr << "write failed" << std::endl;
	else if (static_cast<size_t>(bytes_sent) < len)
		std::cerr << "Warning: Not all data was sent to the client." << std::endl;
	else
		std::cout << "Response sent successfully." << std::endl;
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
	sendData(this->_header.length());
	std::cout << this->_header << std::endl;
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
