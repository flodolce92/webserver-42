#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include <sstream>

class Request
{
private:
	std::string _rawRequest;
	std::istringstream _iss;
	std::string _method;
	std::string _uri;
	std::string _path;
	std::string _queryString;
	std::string _version;
	std::map<std::string, std::string> _headers;
	std::string _body;
	bool _isValid;
	bool _isComplete;

	void parseFirstLine();
	void parseHeaders();
	void parseBody();

public:
	Request(const std::string &rawRequest);
	~Request();

	// Searches the request headers for the given header name and returns a vector
	// containing all values associated with that header.
	// If the header is not found an empty vector is returned.
	std::vector<std::string> getHeaderValues(const std::string &headerName) const;

	// Getters
	const std::string &getMethod() const;
	const std::string &getUri() const;
	const std::string &getPath() const;
	const std::string &getQueryString() const;
	const std::string &getVersion() const;
	const std::map<std::string, std::string> &getHeaders() const;
	const std::string &getBody() const;
	const std::string &getrawRequest() const;
	bool isValid() const;
	bool isComplete() const;

	std::string toString() const;
};

#endif
