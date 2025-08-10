#include <Response.hpp>

static int ft_stoi(std::string s)
{
	int i;
	std::istringstream(s) >> i;
	return i;
}

static std::string extractHostName(const std::string &host)
{
	size_t splitIndex = host.find(':');
	return host.substr(0, splitIndex);
}

static int extractPort(const std::string &host)
{
	size_t splitIndex = host.find(':');
	return ft_stoi(host.substr(splitIndex + 1));
}

void Response::handleGet()
{
	std::string fullPath = FileServer::resolveStaticFilePath(this->_fileName, *this->_matchedLocation);
	this->_fileName = fullPath;
	readFile();
}

void Response::handlePost()
{
	std::cout << "POST" << std::endl;

	std::string body;
	std::string fullPath = this->_matchedLocation->root + "/" + this->_fileName;

	struct stat fileInfo;

	// TODO: Q -> Is this okay? Why can't POST to update?
	if (stat(fullPath.c_str(), &fileInfo) == 0)
	{
		this->_status = StatusCodes::CONFLICT;
		this->_fileName = this->_errorPageFilePath;
		readFile();
		return;
	}

	std::ofstream newFile(fullPath.c_str(), std::ios::out | std::ios::binary);
	if (newFile.is_open())
	{
		std::string newFileContent = body.substr(body.find("\r\n"));
		std::cout << "newFileContent:\n"
				  << newFileContent << std::endl;

		newFile.write(newFileContent.c_str(), newFileContent.length());
		newFile.close();

		this->_status = StatusCodes::CREATED;
		buildResponse();
	}
	else
	{
		this->_status = StatusCodes::INTERNAL_SERVER_ERROR;
		this->_fileName = this->_errorPageFilePath;
		readFile();
	}
}

void Response::handleDelete()
{
	std::cout << "Delete" << std::endl;

	std::string fullPath = FileServer::resolveStaticFilePath(this->_fileName, *this->_matchedLocation);
	std::cout << "Delete Path: " << fullPath << std::endl;

	struct stat fileInfo;
	if (stat(fullPath.c_str(), &fileInfo) == -1)
	{
		this->_status = StatusCodes::NOT_FOUND;
		this->_fileName = this->_errorPageFilePath;
		readFile();
		return;
	}

	if (remove(fullPath.c_str()) == 0)
	{
		this->_status = StatusCodes::NO_CONTENT;
		this->_body = "";
		buildResponse();
	}
	else
	{
		this->_status = StatusCodes::INTERNAL_SERVER_ERROR;
		this->_fileName = this->_errorPageFilePath;
		readFile();
	}
}

void Response::handleUnsupported()
{
	std::cout << "Method not supported: " << this->_method << std::endl;
	this->_status = StatusCodes::METHOD_NOT_ALLOWED;
	this->_fileName = this->_errorPageFilePath;
	readFile();
}

Response::Response(
	const ConfigManager &configManager,
	const Request &request) : _request(request), _configManager(configManager)
{
	// Init basic values
	this->_status = StatusCodes::OK;
	this->_serverConfig = NULL;

	// Validate the request
	std::vector<std::string> host = this->_request.getHeaderValues("host");

	printf("Creating response...\n");

	if (host.size() == 0 || host[0].find(':') == std::string::npos)
	{
		// TODO: Complete this
		printf("Here in error\n");
		this->_status = StatusCodes::BAD_REQUEST;
		return;
	}

	int port = extractPort(host[0]);
	std::string hostName = extractHostName(host[0]);
	this->_host = hostName;
	this->_port = port;

	// std::vector<std::string> filenameValues = this->_request.getHeaderValues("filename");
	// if (filenameValues.size() == 0)
	// {
	// 	// TODO: Complete this
	// 	this->_status = StatusCodes::BAD_REQUEST;
	// 	return;
	// }

	// std::string filename = filenameValues[0];
	// this->_fileName = filename;


	this->_method = this->_request.getMethod();

	printf("Host: %s, Port: %d, Method: %s\n", this->_host.c_str(), this->_port, this->_method.c_str());

	const ServerConfig *server = configManager.findServer(hostName, port);
	if (server == NULL)
	{
		// TODO: Complete this
		this->_status = StatusCodes::INTERNAL_SERVER_ERROR;
		return;
	}
	this->_fileName = request.getUri();
	this->_serverConfig = server;
	const Location *matchedLocation = server->findMatchingLocation(this->_fileName);
	this->_matchedLocation = matchedLocation;
	this->_errorPageFilePath = server->getErrorPage(StatusCodes::METHOD_NOT_ALLOWED.code, *matchedLocation);
	if (configManager.isMethodAllowed(*matchedLocation, this->_method) == false)
	{
		this->_fileName = server->getErrorPage(StatusCodes::METHOD_NOT_ALLOWED.code, *matchedLocation);
		this->_status = StatusCodes::METHOD_NOT_ALLOWED;
		readFile();
		return;
	}

	if (matchedLocation->redirect_code == StatusCodes::MOVED_PERMANENTLY.code && !(matchedLocation->redirect_url.empty()))
	{
		// TODO: Implement this
	}

	if (configManager.isCGIRequest(*matchedLocation, this->_fileName) == true)
	{
		std::string fullPath = FileServer::resolveStaticFilePath(this->_fileName, *matchedLocation);
		std::cout << "TODO: CGI REQUEST HANDLING!" << std::endl;
		// TODO: Handle the ENV variables
		// CGIHandler::executeCGI(fullPath, body, env_var);
		return;
	}

	if (this->_method == "GET")
		this->handleGet();
	else if (this->_method == "POST")
		this->handlePost();
	else if (this->_method == "DELETE")
		this->handleDelete();
	else
		this->handleUnsupported();
}

Response::Response(const Response &src) : _body(src._body),
										  _fullResponse(src._fullResponse),
										  _status(src._status),
										  _request(src._request),
										  _statusCodeMessages(src._statusCodeMessages),
										  _serverConfig(src._serverConfig),
										  _matchedLocation(src._matchedLocation),
										  _fileContent(src._fileContent),
										  _host(src._host),
										  _fileName(src._fileName),
										  _configManager(src._configManager)
{
	std::cout << "Response created as a copy of Response" << std::endl;
}

Response &Response::operator=(const Response &src)
{
	std::cout << "Response assignation operator called" << std::endl;

	// TODO: Think about this
	if (this != &src)
	{
		this->_body = src._body;
		this->_fullResponse = src._fullResponse;
		this->_status = src._status;
		// this->_request = const Request(src._request);
		this->_statusCodeMessages = src._statusCodeMessages;
		this->_serverConfig = src._serverConfig;
		this->_matchedLocation = src._matchedLocation;
		this->_fileContent = src._fileContent;
		this->_host = src._host;
		this->_fileName = src._fileName;
		// this->_configManager = src._configManager;
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

	// Try to get the stats
	if (stat(this->_fileName.c_str(), &fileInfo) == -1)
	{
		std::cerr << "Error getting file stats for '" << this->_fileName << "': " << strerror(errno) << std::endl;
		this->_fileName = this->_errorPageFilePath;
		this->_status = StatusCodes::NOT_FOUND;
		readFile();
		return;
	}

	std::ifstream File(this->_fileName.c_str());

	if (!File.is_open())
	{
		std::cerr << "Error opening file '" << this->_fileName << "'. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
		this->_fileName = this->_errorPageFilePath;
		this->_status = StatusCodes::NOT_FOUND;
		readFile();
		return;
	}

	std::stringstream buffer;
	buffer << File.rdbuf();

	File.close();
	this->_body = buffer.str();
	buildResponse();
}

void Response::buildResponse()
{
	std::ostringstream oss;
	std::string contentType = "Content-Type: text/html; charset=UTF-8\r\n";
	std::string connection = "Connection: close\r\n";
	std::string server = "Server: Webserv/1.0\r\n";
	std::string contentLengthHeader;

	int statusCode = this->_status.code;
	switch (statusCode)
	{
	case 200: // OK - Successful response
		oss << "Content-Length: " << this->_body.length() << "\r\n";
		contentLengthHeader = oss.str();
		this->_fullResponse = "HTTP/1.1 200 OK\r\n" +
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
		this->_fullResponse = "HTTP/1.1 201 Created\r\n" +
							  contentType +
							  contentLengthHeader +
							  connection +
							  server +
							  "\r\n" +
							  this->_body;
		break;

	case 204: // No Content - Request successful, but no content to send (e.g., successful DELETE)
		this->_fullResponse = "HTTP/1.1 204 No Content\r\n" +
							  connection +
							  server +
							  "\r\n" +
							  this->_body;
		break;

	case 400: // Bad Request - Client sent a malformed request
		oss.str("");
		oss << "Content-Length: " << this->_body.length() << "\r\n";
		contentLengthHeader = oss.str();
		this->_fullResponse = "HTTP/1.1 400 Bad Request\r\n" +
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
		this->_fullResponse = "HTTP/1.1 403 Forbidden\r\n" +
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
		this->_fullResponse = "HTTP/1.1 404 Not Found\r\n" +
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
		this->_fullResponse = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n" +
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
		this->_fullResponse = "HTTP/1.1 409 Conflict\r\n" +
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
		this->_fullResponse = "HTTP/1.1 413 Payload Too Large\r\n" +
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
		this->_fullResponse = "HTTP/1.1 500 Internal Server Error\r\n" +
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
		this->_fullResponse = "HTTP/1.1 501 Not Implemented\r\n" +
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
		this->_fullResponse = "HTTP/1.1 504 Gateway Timeout\r\n" +
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
		this->_fullResponse = "HTTP/1.1 500 Internal Server Error\r\n" +
							  contentType +
							  contentLengthHeader +
							  connection +
							  server +
							  "\r\n" +
							  this->_body;
		break;
	}
}

std::string Response::get() const
{
	return this->_fullResponse;
}