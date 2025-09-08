#include <Response.hpp>

static int ft_stoi(std::string s);
static std::string extractHostName(const std::string &host);
static int extractPort(const std::string &host);

Response::Response(
	const ConfigManager &configManager,
	const Request &request) : _request(request),
							  _body(""),
							  _status(StatusCodes::OK),
							  _configManager(configManager),
							  _server(NULL),
							  _errorFound(false),
							  _connectionError(false),
							  _isCGIRequest(false)
{
	// Initialize the Host and Port
	if (this->initPortAndHost() == -1)
	{
		this->_matchedLocation = NULL;
		this->setErrorFilePathForStatus(StatusCodes::INTERNAL_SERVER_ERROR);
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
	this->_matchedLocation = server->findMatchingLocation(this->_request.getPath());
	if (configManager.isMethodAllowed(*this->_matchedLocation, this->getMethod()) == false)
	{
		this->setErrorFilePathForStatus(StatusCodes::METHOD_NOT_ALLOWED);
		this->buildResponseContent();
		return;
	}

	// Redirect if requested
	if (this->_matchedLocation->redirect_code == StatusCodes::MOVED_PERMANENTLY && !(this->_matchedLocation->redirect_url.empty()))
	{
		this->handleRedirect();
		return;
	}

	// Use getPath to trim query string
	ResolutionResult result = FileServer::resolveStaticFilePath(this->_request.getPath(), *this->_matchedLocation);
	this->_filePath = result.path;

	// Check if there was an error in resolving the path
	if (result.pathType == ERROR)
	{
		// Set the error page path based on the status code
		this->setErrorFilePathForStatus(static_cast<StatusCodes::Code>(result.statusCode));
		this->buildResponseContent();
		return;
	}
	else if (result.pathType == DIRECTORY && this->getMethod() == "GET")
	{
		this->_status = StatusCodes::OK;
		this->_body = FileServer::generateDirectoryListing(this->_filePath, this->_request.getPath());
		this->mimeType = FileServer::getMimeType(".html");
		this->buildResponseContent();
		return;
	}

	if (configManager.isCGIRequest(*this->_matchedLocation, this->_filePath) == true)
	{
		this->_isCGIRequest = true;
		this->handleCGI();
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
	this->_errorFound = src._errorFound;
	this->_connectionError = src._connectionError;
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
		this->_errorFound = src._errorFound;
		this->_connectionError = src._connectionError;
	}
	return (*this);
}

Response::~Response()
{
	std::cout << "Response class destroyed" << std::endl;
}

void Response::handleRedirect()
{
	std::string connection = "Connection: ";
	const std::vector<std::string> connectionHeaderValues = this->_request.getHeaderValues("connection");
	if (connectionHeaderValues.size() > 0)
		connection += connectionHeaderValues[0];
	else
		connection += "close";
	connection += "\r\n";

	std::string location = "Location: " + this->_matchedLocation->redirect_url + "\r\n";

	static const std::string server = "Server: Webserv/1.0\r\n";

	this->_body = "<!DOCTYPE html><html><head><title>301 Moved Permanently</title><style>body { font-family: sans-serif; text-align: center; margin-top: 50px; background-color: #f2f2f2; } .container { padding: 20px; border-radius: 10px; background-color: white; display: inline-block; box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2); } h1 { color: #d9534f; }</style></head><body><div class='container'><h1>301 Moved Permanently</h1><p>This document has moved to a new location. Please follow the redirect.</p></div></body></html>";

	std::ostringstream oss;
	std::string contentLengthHeader;

	oss << "Content-Length: " << this->_body.length() << "\r\n";
	contentLengthHeader = oss.str();
	this->_fullResponse = "HTTP/1.1 301 Moved Permanently\r\n" +
						  contentLengthHeader +
						  connection +
						  location +
						  server +
						  "\r\n" +
						  this->_body;
}

void Response::handleCGI()
{
	std::map<std::string, std::string> env_var = this->_request.getHeaders();
	env_var["path_info"] = this->_filePath.substr(0, this->_filePath.find_last_of('/') + 1);
	env_var["script_filename"] = this->_filePath;
	env_var["script_name"] = this->_filePath.substr(this->_filePath.find_last_of('/') + 1);
	env_var["request_method"] = this->getMethod();
	env_var["server_name"] = "Webserv/1.0";
	env_var["query_string"] = this->_request.getQueryString();

	this->_body = CGIHandler::executeCGI(this->_filePath, this->_request.getBody(), env_var);
	this->buildResponseContent();
}

void Response::handleGet()
{
	// The constructor has already called setErrorFilePathForStatus for all ERROR cases
	this->buildResponseContent();
}

void Response::handlePost()
{
	if (this->_request.getHeaders().count("content-type") &&
		this->_request.getHeaders().at("content-type").find("multipart/form-data") != std::string::npos)
	{
		std::string fileName = this->_request.getUploadedFileName();
		std::string fileContent = this->_request.getUploadedFileContent();

		if (fileName.empty() || fileContent.empty())
		{
			this->setErrorFilePathForStatus(StatusCodes::BAD_REQUEST);
			return;
		}

		// Get the upload path from the matched location
		std::string uploadPath = this->_matchedLocation->upload_path;
		std::string fullPath = uploadPath + "/" + fileName;

		if (FileServer::saveFile(fullPath, fileContent))
			this->setErrorFilePathForStatus(StatusCodes::CREATED);
		else
			this->setErrorFilePathForStatus(StatusCodes::INTERNAL_SERVER_ERROR);
	}
	else
	{
		this->setErrorFilePathForStatus(StatusCodes::NOT_IMPLEMENTED);
	}
}

void Response::handleDelete()
{
	if (FileServer::deleteFile(this->_filePath))
	{
		this->_status = StatusCodes::NO_CONTENT;
		this->_body = "File deleted successfully.";
	}
	else
	{
		this->_status = StatusCodes::NOT_FOUND;
		this->setErrorFilePathForStatus(this->_status);
	}
	this->buildResponseContent();
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
	return this->_request.getPath();
}

void Response::readFile()
{
	struct stat fileInfo;
	if (stat(this->_filePath.c_str(), &fileInfo) == -1)
	{
		std::cerr << "Error getting file stats for '" << this->_filePath << "': " << strerror(errno) << std::endl;
		this->setErrorFilePathForStatus(StatusCodes::NOT_FOUND);
		this->readFileError();
		return;
	}

	this->_body = FileServer::readFileContent(this->_filePath);

	if (this->_body.empty())
	{
		this->setErrorFilePathForStatus(StatusCodes::INTERNAL_SERVER_ERROR);
		this->readFileError();
		return;
	}
}

void Response::readFileError()
{
	struct stat fileInfo;
	if (stat(this->_filePath.c_str(), &fileInfo) == -1)
	{
		std::cerr << "Error getting file stats for '" << this->_filePath << "': " << strerror(errno) << std::endl;
		this->setErrorFilePathForStatus(this->_status);
		this->_connectionError = true;
		this->_body = this->generateDynamicErrorPageBody();
		return;
	}

	this->_body = FileServer::readFileContent(this->_filePath);
}

void Response::buildResponseContent()
{
	// Read the file content
	if (this->_errorFound == true)
		this->readFileError();
	if (this->_body.size() == 0 and this->_errorFound == false)
		this->readFile();

	std::ostringstream oss;

	// Build content type
	std::string contentType = "Content-Type: ";
	if (this->_isCGIRequest || this->_errorFound)
		this->mimeType = "text/html";
	if (this->mimeType.size() == 0)
		this->mimeType = FileServer::getMimeType(this->_filePath);
	contentType += this->mimeType;
	contentType += "\r\n";

	std::string connection = "Connection: ";

	const std::vector<std::string> connectionHeaderValues = this->_request.getHeaderValues("connection");
	if (this->_connectionError == true)
		connection += "close";
	else
	{
		if (connectionHeaderValues.size() > 0)
			connection += connectionHeaderValues[0];
		else
			connection += "close";
	}
	connection += "\r\n";

	// Server Information
	static const std::string server = "Server: Webserv/1.0\r\n";

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

std::string Response::extractFileName()
{
	try
	{
		std::string contentDisposition = this->_request.getrawRequest();
		int fromFind = contentDisposition.find("Content-Disposition:");
		int toFind = abs((long)contentDisposition.find("\r\n", fromFind) - fromFind);
		std::string fileNameDirt = contentDisposition.substr(fromFind, toFind);

		fromFind = fileNameDirt.find("filename=");
		std::string fileName = fileNameDirt.substr(fromFind);
		fileName.erase(0, fileName.find_first_of("\"") + 1);
		fileName.erase(fileName.length() - 1, 1);

		return (fileName);
	}
	catch (const std::exception &e)
	{
		return "";
	}
}

void Response::setErrorFilePathForStatus(StatusCodes::Code status)
{
	ResolutionResult result;
	if (this->_errorFound == true)
		return;
	this->_status = status;
	this->_errorFound = true;
	if (!this->_server || !this->_matchedLocation)
		result = ServerConfig::getEmptyResolutionResult(this->_status);
	else
		result = this->_server->getErrorPage(this->_status, *this->_matchedLocation);

	// If a custom error page was found, use its path
	// If no custom error page found, response should be a static error string
	if (result.pathType != ERROR)
		this->_filePath = result.path;
	else
		this->_filePath = "";
}

std::string Response::generateDynamicErrorPageBody() const
{
	std::ostringstream oss;
	std::string message = StatusCodes::getMessage(this->_status);
	int statusCode = static_cast<int>(this->_status);

	oss << "<!DOCTYPE html><html><head><title>Error " << statusCode << "</title><style>body { font-family: sans-serif; text-align: center; margin-top: \
			50px; background-color: #f2f2f2; } .container { padding: 20px; border-radius: 10px; background-color: white; display: inline-block; box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2); } \
			h1 { color: #d9534f; }</style></head><body><div class='container'><h1>"
		<< statusCode << " " << message << "</h1><p> \
			The server encountered an unexpected condition that prevented it from fulfilling the request.</p></div></body></html>";

	return oss.str();
}
