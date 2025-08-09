#include <Response.hpp>

static std::string extractHostName(const std::string &host)
{
    size_t splitIndex = host.find(':');
    return host.substr(0, splitIndex);
}

static int extractPort(const std::string &host)
{
    size_t splitIndex = host.find(':');
    return stoi(host.substr(splitIndex));
}

Response::Response(
    const ConfigManager &configManager,
    const Request &request) : _code(200), _request(request)
{
    std::vector<std::string> host = this->_request.getHeaderValues("host");
    if (host.size() == 0 || host[0].find(':') == std::string::npos)
    {
        // TODO: Return a 404 response
        return;
    }

    int port = extractPort(host[0]);
    std::string hostName = extractHostName(host[0]);
    this->_host = hostName;

    std::vector<std::string> filenameValues = this->_request.getHeaderValues("filename");
    if (filenameValues.size() == 0)
    {
        // TODO: Return a 404
        return;
    }
    std::string filename = filenameValues[0];
    this->_fileName = filename;
    std::string method = this->_request.getMethod();

    std::cout << "Host and Port: " << hostName << port << std::endl;
    const ServerConfig *server = configManager.findServer(hostName, port);
    const Location *matchedLocation = server->findMatchingLocation(filename);

    if (configManager.isMethodAllowed(*matchedLocation, method) == false)
    {
        this->_fileName = FileServer::resolveStaticFilePath("error/error.html", *matchedLocation); // TODO: Think about how to create an enum with this
        this->_code = 405;
        readFile();
        return;
    }

    if (matchedLocation->redirect_code == 301 and !(matchedLocation->redirect_url.empty()))
    {
        // TODO: Implement this
    }

    if (configManager.isCGIRequest(*matchedLocation, filename) == true)
    {
        std::string fullPath = FileServer::resolveStaticFilePath(filename, *matchedLocation);
        std::cout << "TODO: CGI REQUEST HANDLING!" << std::endl;
        // TODO: Handle the ENV variables
        // CGIHandler::executeCGI(fullPath, body, env_var);
        return;
    }

    if (method == "GET")
    {
        std::string fullPath = FileServer::resolveStaticFilePath(filename, *matchedLocation);
        this->_fileName = fullPath;
        readFile();
    }
    else if (method == "POST")
    {
        std::cout << "POST" << std::endl;

        std::string body;
        std::string fullPath = matchedLocation->root + "/" + this->_fileName;

        struct stat fileInfo;
        if (stat(fullPath.c_str(), &fileInfo) == 0)
        {
            this->_code = 409;
            this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedLocation);
            readFile();
        }
        else
        {
            std::ofstream newFile(fullPath.c_str(), std::ios::out | std::ios::binary);
            if (newFile.is_open())
            {
                std::string newFileContent = body.substr(body.find("\r\n"));
                std::cout << "newFileContent:\n"
                          << newFileContent << std::endl;

                newFile.write(newFileContent.c_str(), newFileContent.length());
                newFile.close();

                this->_code = 201;
                setHeader(this->_code);
                // this->_client->appendToWriteBuffer(this->_header);
            }
            else
            {
                this->_code = 500;
                this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedLocation);
                readFile();
            }
        }
    }
    else if (method == "DELETE")
    {
        std::cout << "Delete" << std::endl;
        std::string fullPath = FileServer::resolveStaticFilePath(filename, *matchedLocation);
        std::cout << "Delete Path: " << fullPath << std::endl;

        struct stat fileInfo;
        if (stat(fullPath.c_str(), &fileInfo) == -1)
        {
            this->_code = 404;
            this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedLocation);
            readFile();
            return;
        }

        if (remove(fullPath.c_str()) == 0)
        {
            this->_code = 204;
            this->_body = "";
            setHeader(this->_code);
            // this->_client->appendToWriteBuffer(this->_header);
        }
        else
        {
            this->_code = 500;
            this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedLocation);
            readFile();
        }
    }
    else
    {
        std::cout << "Method not supported: " << method << std::endl;
        this->_code = 501;
        this->_fileName = FileServer::resolveStaticFilePath("error.html", *matchedLocation);
        readFile();
    }
}

Response::Response(const Response &src) : _code(src._code),
                                          _message(src._message),
                                          _header(src._header),
                                          _request(src._request),
                                          _fileName(src._fileName)
{
    std::cout << "Response created as a copy of Response" << std::endl;
}

Response &Response::operator=(const Response &src)
{
    std::cout << "Response assignation operator called" << std::endl;
    if (this != &src)
    {
        this->_code = src._code;
        this->_fileName = src._fileName;
        this->_message = src._message;
        this->_header = src._header;
        this->_host = src._host;
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
        return;
    }

    int fd = open(this->_fileName.c_str(), O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "Error opening file '" << this->_fileName << "'. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
        this->_fileName = "var/www/html/error/error.html";
        this->_code = 404;
        readFile();
        return;
    }

    char *message = new char[fileInfo.st_size + 1];

    ssize_t bytes_read;
    bytes_read = read(fd, message, fileInfo.st_size);
    if (bytes_read == -1)
    {
        std::cerr << "Error reading file: " << this->_fileName << std::endl;
        delete[] message;
        close(fd);
        return;
    }
    message[bytes_read] = '\0';
    close(fd);
    this->_body = message;
    setHeader(this->_code);
    // this->_client->appendToWriteBuffer(this->_header);
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

void Response::initializeStatusCodeMessages()
{
}

std::ostream &operator<<(std::ostream &os, const Response &response)
{
    os << "Response:" << std::endl;
    os << "  Code: " << response.getCode() << std::endl;
    os << "  Host: " << response.getHost() << std::endl;
    os << "  Filename: " << response.getFileName() << std::endl;
    os << "  Header size: " << response.getHeader().size() << " bytes" << std::endl;
    os << "  Body size: " << response.getBody().size() << " bytes" << std::endl;
    return os;
}

int Response::getCode() const
{
    return this->_code;
}

char *Response::getMessage() const
{
    return this->_message;
}

std::string Response::getBody() const
{
    return this->_body;
}

std::string Response::getHeader() const
{
    return this->_header;
}

const Request &Response::getRequest() const
{
    return this->_request;
}

std::map<int, std::string> Response::getStatusCodeMessages() const
{
    return this->_statusCodeMessages;
}

std::string Response::getHost() const {
    return this->_host;
}

std::string Response::getFileName() const {
    return this->_fileName;
}
