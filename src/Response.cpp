#include <Response.hpp>

static int stoi( std::string s );
static std::string extractFilename(const std::string& requestBody);

Response::Response( ClientConnection *client, const ConfigManager & configManager ):
	_code(200),
	_client(client)
{
	std::string request_str(client->getReadBuffer());
	std::string cleanRequest = request_str.substr(0, request_str.find("\n"));

	std::string host = request_str.substr(request_str.find("Host:") + 6, request_str.find("\r\n"));

	size_t pos = 0;
	std::string sub;
	std::vector<std::string> vectorRequest;
	while ((pos = cleanRequest.find(' ')) != std::string::npos)
	{
		sub = cleanRequest.substr(0, pos);
		vectorRequest.push_back(sub);
		cleanRequest.erase(0, pos + 1);
	}
	vectorRequest.push_back(cleanRequest);

	std::string method = vectorRequest[0];
	std::string filename = vectorRequest[1];
	
	int port = stoi(host.substr(host.find(":") + 1, host.find("\n")));
	std::string hostName = host.substr(0, host.find(":"));
	
	const ServerConfig *server = configManager.findServer(hostName, port);
	const Route *matchedRoute = server->findMatchingRoute(filename);
	
	if (configManager.isMethodAllowed(*matchedRoute, method) == false)
	{
		this->_fileName = FileServer::resolveStaticFilePath("error/error.html", *matchedRoute);
		this->_code = 405;
		readFile();
		return ;
	}

	if (matchedRoute->redirect_code == 301 and !(matchedRoute->redirect_url.empty()))
	{
		// do this
	}

	if (configManager.isCGIRequest(*matchedRoute, filename) == true)
	{
		std::string fullPath = FileServer::resolveStaticFilePath(filename, *matchedRoute);
		std::string body;
        size_t body_pos = request_str.find("\r\n\r\n");
        if (body_pos != std::string::npos)
            body = request_str.substr(body_pos + 4);
		else
		{
			this->_code = 400;
			this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedRoute);
			readFile();
			return;
		}
		// come le prende le variabili di ambiente?
		// CGIHandler::executeCGI(fullPath, body, env_var);
		return ;
	}

	if (method == "GET")
	{
		std::string fullPath = FileServer::resolveStaticFilePath(filename, *matchedRoute);
		this->_fileName = fullPath;
		readFile();
	}
	else if (method == "POST")
	{
		std::cout << "POST" << std::endl;

        std::string body;
        size_t body_pos = request_str.find("\r\n\r\n");
        if (body_pos != std::string::npos)
            body = request_str.substr(body_pos + 4);
		else
		{
			this->_code = 400;
			this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedRoute);
			readFile();
			return;
		}

        std::string fullPath = matchedRoute->root + "/" + extractFilename(body);

        struct stat fileInfo;
        if (stat(fullPath.c_str(), &fileInfo) == 0)
		{
            this->_code = 409;
            this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedRoute);
            readFile();
        }
		else
		{
            std::ofstream newFile(fullPath.c_str(), std::ios::out | std::ios::binary);
            if (newFile.is_open())
			{
				std::string newFileContent = body.substr(body.find("\r\n"));
				std::cout << "newFileContent:\n" << newFileContent << std::endl;

                newFile.write(newFileContent.c_str(), newFileContent.length());
                newFile.close();

                this->_code = 201;
                setHeader(this->_code);
                this->_client->appendToWriteBuffer(this->_header);
            }
			else
			{
                this->_code = 500;
                this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedRoute);
                readFile();
            }
        }
	}
	else if (method == "DELETE")
	{
		std::cout << "Delete" << std::endl;
        std::string fullPath = FileServer::resolveStaticFilePath(filename, *matchedRoute);
		std::cout << "Delete Path: " << fullPath << std::endl;

        struct stat fileInfo;
        if (stat(fullPath.c_str(), &fileInfo) == -1)
		{
            this->_code = 404;
            this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedRoute);
            readFile();
            return;
        }

        if (remove(fullPath.c_str()) == 0)
		{
            this->_code = 204;
			this->_body = "";
            setHeader(this->_code);
            this->_client->appendToWriteBuffer(this->_header);
        }
		else
		{
            this->_code = 500;
            this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedRoute);
            readFile();
        }
	}
	else
	{
		std::cout << "Method not supported: " << method << std::endl;
		this->_code = 501;
		this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedRoute);
		readFile();
	}
}

static std::string extractFilename(const std::string& requestBody)
{
    const std::string filename_marker = "filename=\"";

    size_t start_pos = requestBody.find(filename_marker);
	
    if (start_pos == std::string::npos) 
		return "";
	
    start_pos += filename_marker.length();
	
    size_t end_pos = requestBody.find('"', start_pos);
	
    if (end_pos == std::string::npos)
		return "";
	
	std::cout << requestBody.substr(start_pos, end_pos - start_pos) << std::endl;
    return requestBody.substr(start_pos, end_pos - start_pos);
}

static int stoi( std::string s )
{
	int i;
	std::istringstream(s) >> i;
	return i;
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

	if (stat(this->_fileName.c_str(), &fileInfo) == -1)
	{
		std::cerr << "Error getting file stats for '" << this->_fileName << "': " << strerror(errno) << std::endl;
		this->_fileName = "var/www/html/error/error.html";
		this->_code = 404;
		readFile();
		return ;
	}

	int fd = open(this->_fileName.c_str(), O_RDONLY);
	if (fd == -1)
	{
		std::cerr << "Error opening file '" << this->_fileName << "'. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
		this->_fileName = "var/www/html/error/error.html";
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
