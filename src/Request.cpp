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
	// Check Content-Type header to know how to parse the body
	std::string contentType;
	if (this->getHeaders().count("content-type"))
		contentType = this->getHeaders().at("content-type");

	if (contentType.find("multipart/form-data") != std::string::npos)
	{
		// File upload handling
		parseMultipartBody(contentType);
	}
	else
	{
		std::stringstream ss;
		ss << _iss.rdbuf();
		_body = ss.str();
	}
	_isComplete = true;
	_isValid = true;
}

void Request::parseMultipartBody(const std::string &contentType)
{
	// Extracting the boundary string to delimit parts
	size_t boundaryPos = contentType.find("boundary=");
	if (boundaryPos == std::string::npos)
	{
		_isValid = false;
		return;
	}
	std::string boundary = "--" + contentType.substr(boundaryPos + 9);

	// Reading the complete multipart body
	std::stringstream ss;
	ss << _iss.rdbuf();
	std::string rawBody = ss.str();

	// Finding the beginning and end of the file part
	size_t startPos = rawBody.find(boundary);
	if (startPos == std::string::npos)
	{
		_isValid = false;
		return;
	}

	size_t endPos;
	while (startPos != std::string::npos)
	{
		startPos = rawBody.find("\r\n\r\n", startPos) + 4; // Move past the part headers

		endPos = rawBody.find(boundary, startPos);
		if (endPos == std::string::npos)
		{
			_isValid = false;
			return;
		}

		// Extracting the headers for the current part
		std::string partHeaders = rawBody.substr(rawBody.find(boundary) + boundary.length() + 2, startPos - (rawBody.find(boundary) + boundary.length() + 2));

		// Checking if this part contains a file
		if (partHeaders.find("filename=") != std::string::npos)
		{
			// Extracting the filename
			size_t filenamePos = partHeaders.find("filename=");
			size_t filenameStart = partHeaders.find("\"", filenamePos) + 1;
			size_t filenameEnd = partHeaders.find("\"", filenameStart);
			_uploadedFileName = partHeaders.substr(filenameStart, filenameEnd - filenameStart);

			// Extracting the file content
			_uploadedFileContent = rawBody.substr(startPos, endPos - startPos);

			_isValid = true;
			return;
		}
		startPos = endPos;
	}

	_isValid = true;
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
const std::string &Request::getUploadedFileName() const { return _uploadedFileName; }
const std::string &Request::getUploadedFileContent() const { return _uploadedFileContent; }
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
