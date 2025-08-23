#include <Response.hpp>

static int ft_stoi(std::string s);
static std::string extractHostName(const std::string &host);
static int extractPort(const std::string &host);


Response::Response(
    const ConfigManager &configManager,
    const Request &request) :
    _request(request),
    _body(""),
    _status(StatusCodes::OK),
    _configManager(configManager),
    _server(NULL)
{
	// Initialize the Host and Port
	if (this->initPortAndHost() == -1)
	{
		this->setErrorFilePathForStatus(StatusCodes::BAD_REQUEST); // TODO: Change to malformed
		this->buildResponseContent();
		return;
	}

	const ServerConfig *server = configManager.findServer(this->_host, this->_port);
	if (server == NULL)
	{
		this->setErrorFilePathForStatus(StatusCodes::INTERNAL_SERVER_ERROR);
		this->buildResponseContent();
		return;
	}
	this->_server = server;

	// Set matched location and check if method allowed
	this->_matchedLocation = server->findMatchingLocation(this->_request.getUri());
	if (configManager.isMethodAllowed(*this->_matchedLocation, this->getMethod()) == false)
	{
		this->setErrorFilePathForStatus(StatusCodes::METHOD_NOT_ALLOWED);
		this->buildResponseContent();
		return;
	}

	// Redirect if requested
	if (this->_matchedLocation->redirect_code == StatusCodes::MOVED_PERMANENTLY 
		&& !(this->_matchedLocation->redirect_url.empty()))
	{
		// TODO: Implement this
	}


	// TODO: Remember to change the Uri (do not include the query string)
	this->_filePath = FileServer::resolveStaticFilePath(this->_request.getUri(), *this->_matchedLocation);

	
	if (configManager.isCGIRequest(*this->_matchedLocation, this->_filePath) == true)
	{
		std::string fullPath = FileServer::resolveStaticFilePath(this->_filePath, *this->_matchedLocation);
		std::cout << "TODO: CGI REQUEST HANDLING!" << std::endl;
		std::map<std::string, std::string> env_var = this->_request.getHeaders();
		env_var["path_info"] = this->_filePath.substr(0, this->_filePath.find_last_of('/') + 1);
		env_var["script_filename"] = this->_filePath;
		env_var["script_name"] = this->_filePath.substr(this->_filePath.find_last_of('/') + 1);
		env_var["request_method"] = this->getMethod();
		env_var["server_name"] =  "Webserv/1.0";
		env_var["query_string"] = this->_request.getUri();

		// CGIHandler::executeCGI(fullPath, this->_request.getBody(), env_var);
		return;
	}

	if (this->getMethod() == "GET")
		this->handleGet();
	else if (this->getMethod() == "POST")
		this->handlePost();
	else if (this->getMethod() == "DELETE")
		this->handleDelete();
	else
		this->handleUnsupported();
}

Response::Response(const Response &src) : _request(src._request), _configManager(src._configManager)
{
	std::cout << "Response created as a copy of Response" << std::endl;

	this->_body = src._body;
    this->_status = src._status;
    this->mimeType = src.mimeType;
    this->_fullResponse = src._fullResponse;
    this->_host = src._host;
    this->_port = src._port;
    this->_server = src._server;
    this->_errorPageFilePath = src._errorPageFilePath;
    this->_filePath = src._filePath;
    this->_statusCodeMessages = src._statusCodeMessages;
    this->_matchedLocation = src._matchedLocation;
}

Response &Response::operator=(const Response &src)
{
	std::cout << "Response assignation operator called" << std::endl;

	if (this != &src)
	{
		this->_body = src._body;
        this->_status = src._status;
        this->mimeType = src.mimeType;
        this->_fullResponse = src._fullResponse;
        this->_host = src._host;
        this->_port = src._port;
        this->_server = src._server;
        this->_errorPageFilePath = src._errorPageFilePath;
        this->_filePath = src._filePath;
        this->_statusCodeMessages = src._statusCodeMessages;
        this->_matchedLocation = src._matchedLocation;
	}
	return (*this);
}

Response::~Response()
{
	std::cout << "Response class destroyed" << std::endl;
}

void Response::handleGet()
{
	if (this->_filePath.size() == 0) {
		
		if (!this->_server->autoindex) {
			this->setErrorFilePathForStatus(StatusCodes::FORBIDDEN);
		}
		else {
			this->setErrorFilePathForStatus(StatusCodes::NOT_FOUND);
		}
	}
	this->buildResponseContent();
}

void Response::handlePost()
{
	std::cout << "POST" << std::endl;

	std::string fullPath = this->_matchedLocation->root + "/" + this->_filePath;

	struct stat fileInfo;

	// TODO: Q -> Is this okay? Why can't POST to update?
	if (stat(fullPath.c_str(), &fileInfo) == 0)
	{
		this->setErrorFilePathForStatus(StatusCodes::CONFLICT);
		this->buildResponseContent();
		return;
	}

	std::ofstream newFile(fullPath.c_str(), std::ios::out | std::ios::binary);
	if (newFile.is_open())
	{
		std::string newFileContent = this->_request.getBody();
		newFile.write(newFileContent.c_str(), newFileContent.length());
		newFile.close();
		this->setErrorFilePathForStatus(StatusCodes::CREATED);
		this->buildResponseContent();
	}
	else
	{
		this->setErrorFilePathForStatus(StatusCodes::INTERNAL_SERVER_ERROR);
		this->buildResponseContent();
	}
}

// TODO: Implement
void Response::handleDelete()
{
	std::cout << "Delete" << std::endl;

	// std::string fullPath = FileServer::resolveStaticFilePath(this->_filePath, *this->_matchedLocation);
	// std::cout << "Delete Path: " << fullPath << std::endl;

	// struct stat fileInfo;
	// if (stat(fullPath.c_str(), &fileInfo) == -1)
	// {
	// 	this->_status = StatusCodes::NOT_FOUND;
	// 	this->_filePath = this->_errorPageFilePath;
	// 	readFile();
	// 	return;
	// }

	// if (remove(fullPath.c_str()) == 0)
	// {
	// 	this->_status = StatusCodes::NO_CONTENT;
	// 	this->_body = "";
	// 	buildResponseContent();
	// }
	// else
	// {
	// 	this->_status = StatusCodes::INTERNAL_SERVER_ERROR;
	// 	this->_filePath = this->_errorPageFilePath;
	// 	readFile();
	// }
}

void Response::handleUnsupported()
{
	this->setErrorFilePathForStatus(StatusCodes::NOT_IMPLEMENTED);
	this->buildResponseContent();
}

int Response::initPortAndHost()
{
	std::vector<std::string> hostVals = this->_request.getHeaderValues("host");

	if (hostVals.size() == 0 || hostVals[0].find(':') == std::string::npos)
	{
		return (-1);
	}

	this->_port = extractPort(hostVals[0]);
	this->_host = extractHostName(hostVals[0]);
	return (0);
}

std::string Response::getMethod() const
{
	return this->_request.getMethod();
}

std::string Response::getFileName() const
{
	return this->_request.getUri();
}

void Response::readFile()
{
	// For status codes that already have error pages or generated content, proceed with reading

	printf("Loop\n\n");

	struct stat fileInfo;
	if (stat(this->_filePath.c_str(), &fileInfo) == -1)
    {
        std::cerr << "Error getting file stats for '" << this->_filePath << "': " << strerror(errno) << std::endl;
        this->setErrorFilePathForStatus(StatusCodes::NOT_FOUND);
        this->readFile();
        return;
    }
    
    // Check if the path is a directory
    if (S_ISDIR(fileInfo.st_mode))
    {
		if (this->_matchedLocation->autoindex) {
			this->_body = FileServer::generateDirectoryListing(this->_filePath);
			this->mimeType = FileServer::getMimeType(".html");
			this->buildResponseContent();
			return;
		}
        // Handle directory case
        std::cerr << "Path is a directory: '" << this->_filePath << "'" << std::endl;
        this->setErrorFilePathForStatus(StatusCodes::FORBIDDEN);
        this->readFile();
        return;
    }

	this->_body = FileServer::readFileContent(this->_filePath);
}

void Response::buildResponseContent() {
	// Read the file content
	if (this->_body.size() == 0) {
		this->readFile();
	}

	std::ostringstream oss;

	// Build content type
	std::string contentType = "Content-Type: ";
	if (this->mimeType.size() == 0)
		std::string mimeType = FileServer::getMimeType(this->_filePath);
	contentType += mimeType;
	contentType += "\r\n";

	std::string connection = "Connection: ";

	const std::vector<std::string> connectionHeaderValues = this->_request.getHeaderValues("connection");
	if (connectionHeaderValues.size() > 0)
		connection += connectionHeaderValues[0];
	else
		connection += "close";
	connection += "\r\n";

	// Server Information
	std::string server = "Server: Webserv/1.0\r\n"; // TODO: Move to a static const

	std::string contentLengthHeader;

	switch (this->_status)
	{
		case StatusCodes::OK: // OK - Successful response
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

		case StatusCodes::CREATED: // Created - Resource successfully created
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

		case StatusCodes::NO_CONTENT: // No Content - Request successful, but no content to send (e.g., successful DELETE)
			this->_fullResponse = "HTTP/1.1 204 No Content\r\n" +
								connection +
								server +
								"\r\n" +
								this->_body;
			break;

		case StatusCodes::BAD_REQUEST: // Bad Request - Client sent a malformed request
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

		case StatusCodes::FORBIDDEN: // Forbidden - Client doesn't have access rights
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

		case StatusCodes::NOT_FOUND: // Not Found - Resource not found
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

		case StatusCodes::METHOD_NOT_ALLOWED: // Method Not Allowed - HTTP method not supported for resource
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

		case StatusCodes::CONFLICT: // Conflict - Request could not be completed due to a conflict
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

		case StatusCodes::PAYLOAD_TOO_LARGE: // Payload Too Large - Request entity is larger than limits defined by server
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

		case StatusCodes::INTERNAL_SERVER_ERROR: // Internal Server Error - Generic server-side error
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

		case StatusCodes::NOT_IMPLEMENTED: // Not Implemented - Server does not support the functionality required to fulfill the request
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

		case StatusCodes::GATEWAY_ERROR: // Gateway Timeout - The server, while acting as a gateway or proxy, did not get a response from an upstream server in time
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

bool Response::hasError() const
{
	return !this->_request.isValid() || this->_status == StatusCodes::BAD_REQUEST; // TODO: Add more cases
}

// Static Helper Methods
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

void Response::setErrorFilePathForStatus(StatusCodes::Code status) {
	this->_status = status;
	this->_filePath = this->_server->getErrorPage(this->_status, *this->_matchedLocation);
}