#include "Response.hpp"

Response::Response( int socket, int code, const char* fileName ) :
	_socket(socket),
	_code(code),
	_fileName(fileName)
{
	std::cout << "Response class created" << "\n";
}

Response::Response( const Response& src ) :
	_socket(src._socket),
	_code(src._code),
	_fileName(src._fileName),
	_message(src._message),
	_header(src._header)
{
	std::cout << "Response created as a copy of Response" << "\n";
}

Response& Response::operator=( const Response& src )
{
	std::cout << "Response assignation operator called" << "\n";
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
	std::cout << "Response class destroyed" << "\n";
}

void Response::sendData(size_t len)
{
	long bytes_sent = write(this->_socket, this->_message, len);
	if (bytes_sent < 0)
		std::cerr << "write failed" << "\n";
	else if (static_cast<size_t>(bytes_sent) < len)
		std::cerr << "Warning: Not all data was sent to the client." << "\n";
	else
		std::cout << "Response sent successfully." << "\n";
}

void Response::readFile()
{
	struct stat fileInfo;
	if (stat(this->_fileName, &fileInfo) == -1)
	{
		std::cerr << "Error getting file stats for '" << this->_fileName << "': " << strerror(errno) << "\n";
		return ;
	}

	int fd = open(this->_fileName, O_RDONLY);
	if (fd == -1)
	{
		std::cerr << "Error opening file '" << this->_fileName << "'. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
		return ;
	}
	
	this->_header = "HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Connection: close\r\n"
		"Server: Webserv/1.0\r\n"
		"\r\n";
		
	size_t headerLength = this->_header.length();
	size_t totalBufferSize = headerLength + fileInfo.st_size;
	this->_message = new char[totalBufferSize];

	const char* headerChar = this->_header.c_str();
	for (size_t i = 0; i < headerLength; i++)
		this->_message[i] = headerChar[i];
	
	ssize_t bytes_read;
	bytes_read = read(fd, this->_message + headerLength, fileInfo.st_size);
	if (bytes_read == -1)
	{
		std::cerr << "Error reading file: " << this->_fileName << std::endl;
		delete[] this->_message;
		close(fd);
		return ;
	}
	close(fd);
	this->_message[bytes_read] = '\0';
	sendData(totalBufferSize);
	delete[] this->_message;
}
