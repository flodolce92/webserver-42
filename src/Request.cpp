#include <Request.hpp>
#include <iostream>
#include <cstdlib>
#include <errno.h>
#include <cctype>

static std::string toLower(const std::string &str)
{
	std::string lowerStr = str;
	for (size_t i = 0; i < lowerStr.length(); ++i)
		lowerStr[i] = std::tolower(lowerStr[i]);
	return lowerStr;
}

static std::string trim(const std::string &str)
{
	size_t start = str.find_first_not_of(" \t\n\r");
	if (start == std::string::npos)
		return "";

	size_t end = str.find_last_not_of(" \t\n\r");
	return str.substr(start, end - start + 1);
}

Request::Request(const std::string &rawRequest)
	: _rawRequest(rawRequest),
	  _iss(_rawRequest),
	  _isValid(false),
	  _isComplete(false)
{
	parseFirstLine();
	if (!_isValid)
		return;

	parseHeaders();
	if (!_isValid)
		return;

	parseBody();
	if (!_isValid)
		return;
}

Request::~Request() {}

void Request::parseFirstLine()
{
	std::string line;
	std::getline(_iss, line);
	line = trim(line);

	std::istringstream lineStream(line);
	lineStream >> _method >> _uri >> _version;

	if (_method.empty() || _uri.empty() || _version.empty() ||
		_version != "HTTP/1.1")
	{
		_isValid = false;
	}
	else
		_isValid = true;

	_path = _uri;
	size_t queryPos = _uri.find('?');
	if (queryPos != std::string::npos)
	{
		_path = _uri.substr(0, queryPos);
		_queryString = _uri.substr(queryPos + 1);
	}
}

void Request::parseHeaders()
{
	std::string line;
	while (std::getline(_iss, line) && line != "\r")
	{ // Read until an empty line
		size_t colonPos = line.find(':');
		if (colonPos != std::string::npos)
		{
			std::string headerName = line.substr(0, colonPos);
			std::string headerValue = line.substr(colonPos + 1);

			headerValue = trim(headerValue);
			headerName = trim(headerName);
			headerName = toLower(headerName);

			if (headerName.empty() || headerValue.empty())
			{
				_isValid = false;
				return;
			}
			_headers[headerName] = headerValue;
		}
	}
}

void Request::parseBody()
{
	if (_headers.count("content-length"))
	{
		char *end;
		errno = 0;
		unsigned long contentLength = strtoul(_headers.at("content-length").c_str(), &end, 10);

		if (errno == ERANGE || *end != '\0')
		{
			_isValid = false;
			return;
		}

		size_t bodyStartPos = _iss.tellg();
		size_t rawLength = _rawRequest.length();

		if (rawLength >= bodyStartPos + contentLength)
		{
			_body = _rawRequest.substr(bodyStartPos, contentLength);
			_isComplete = true;
		}
		else
			_isComplete = false;
	}
	else
		_isComplete = true;
}

// Getters
const std::string &Request::getMethod() const { return _method; }
const std::string &Request::getUri() const { return _uri; }
const std::string &Request::getPath() const { return _path; }
const std::string &Request::getQueryString() const { return _queryString; }
const std::string &Request::getVersion() const { return _version; }
const std::map<std::string, std::string> &Request::getHeaders() const { return _headers; }
const std::string &Request::getBody() const { return _body; }
const std::string &Request::getrawRequest() const { return _rawRequest; }
bool Request::isValid() const { return _isValid; }
bool Request::isComplete() const { return _isComplete; }

std::vector<std::string> Request::getHeaderValues(const std::string &headerName) const
{
	std::vector<std::string> values;
	std::map<std::string, std::string>::const_iterator it = _headers.find(headerName);

	if (it != _headers.end())
	{
		std::string rawValues = it->second;
		std::stringstream ss(rawValues);
		std::string value;

		while (std::getline(ss, value, ','))
		{
			value = trim(value);
			if (!value.empty())
				values.push_back(value);
		}
	}
	return values;
}

std::string Request::toString() const
{
	std::ostringstream oss;

	// Request line
	oss << _method << " " << _uri << " " << _version << "\r\n";

	// Headers
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
		 it != _headers.end(); ++it)
	{
		oss << it->first << ": " << it->second << "\r\n";
	}

	// Empty line separating headers from body
	oss << "\r\n";

	// Body (if present)
	if (!_body.empty())
		oss << _body;

	return oss.str();
}
