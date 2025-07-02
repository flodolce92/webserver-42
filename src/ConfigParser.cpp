#include <ConfigParser.hpp>
#include <cctype>

// Route
Route::Route()
	: path(""),
	  allowed_methods(),
	  redirect_code(0),
	  redirect_url(""),
	  redirect(""),
	  root(""),
	  directory_listing(false),
	  index(""),
	  index_files(),
	  cgi_extension(""),
	  cgi_path(""),
	  upload_path(""),
	  upload_enabled(false)
{
}

// ServerConfig
ServerConfig::ServerConfig()
	: host("localhost"),
	  port(80),
	  server_names(),
	  error_pages(),
	  client_max_body_size(1048576),
	  routes()
{
}

// Config
Config::Config() : servers() {}

// ConfigParser
ConfigParser::ConfigParser() : pos(0), line_num(1) {}
ConfigParser::~ConfigParser() {}

Config ConfigParser::parseFile(const std::string &filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw std::runtime_error("Cannot open configuration file: " + filename);

	std::stringstream buffer;
	buffer << file.rdbuf();
	content = buffer.str();
	file.close();
	pos = 0;
	line_num = 1;
	Config config = parseConfig();
	validateConfig(config);
	return config;
}

Config ConfigParser::parseString(const std::string &config_content)
{
	content = config_content;
	pos = 0;
	line_num = 1;
	Config config = parseConfig();
	validateConfig(config);
	return config;
}

void ConfigParser::skipWhitespace()
{
	while (!isAtEnd() && (content[pos] == ' ' || content[pos] == '\t' ||
						  content[pos] == '\n' || content[pos] == '\r'))
	{
		if (content[pos] == '\n')
			line_num++;
		pos++;
	}
}

void ConfigParser::skipComments()
{
	if (!isAtEnd() && content[pos] == '#')
	{
		while (!isAtEnd() && content[pos] != '\n')
			pos++;
	}
}

std::string ConfigParser::getRestOfLine()
{
	skipWhitespace();

	if (isAtEnd())
		return "";

	std::string value;
	while (!isAtEnd() && content[pos] != ';' && content[pos] != '\n' && content[pos] != '\r')
	{
		value += content[pos];
		pos++;
	}
	return trim(value);
}

std::string ConfigParser::getNextToken()
{
	skipWhitespace();
	skipComments();
	skipWhitespace();

	if (isAtEnd())
		return "";

	std::string token;
	if (content[pos] == '"' || content[pos] == '\'')
	{
		char quote = content[pos];
		pos++;
		while (!isAtEnd() && content[pos] != quote)
		{
			if (content[pos] == '\\' && pos + 1 < content.length())
				pos++;
			token += content[pos];
			pos++;
		}
		if (isAtEnd())
			throwError("Unterminated quoted string");
		pos++;
	}
	else
	{
		while (!isAtEnd() && content[pos] != ' ' && content[pos] != '\t' &&
			   content[pos] != '\n' && content[pos] != '\r' && content[pos] != ';' &&
			   content[pos] != '{' && content[pos] != '}' && content[pos] != '#')
		{
			token += content[pos];
			pos++;
		}
	}
	return token;
}

bool ConfigParser::isAtEnd() const
{
	return pos >= content.length();
}

void ConfigParser::throwError(const std::string &message) const
{
	std::ostringstream oss;
	oss << "Configuration error at line " << line_num << ": " << message;
	throw std::runtime_error(oss.str());
}

Config ConfigParser::parseConfig()
{
	Config config;

	while (!isAtEnd())
	{
		std::string token = getNextToken();
		if (token.empty())
			break;

		if (token == "server")
			config.servers.push_back(parseServer());
		else
			throwError("Expected 'server' directive, got: " + token);
	}

	if (config.servers.empty())
		throwError("No server blocks found in configuration");
	return config;
}

ServerConfig ConfigParser::parseServer()
{
	ServerConfig server;

	skipWhitespace();
	if (isAtEnd() || content[pos] != '{')
		throwError("Expected '{' after 'server'");

	pos++;
	while (!isAtEnd())
	{
		skipWhitespace();
		skipComments();
		skipWhitespace();
		if (isAtEnd())
			throwError("Unexpected end of file, expected '}'");

		if (content[pos] == '}')
		{
			pos++;
			break;
		}
		std::string directive = getNextToken();
		if (directive.empty())
			throwError("Empty directive");

		if (directive == "location")
			server.routes.push_back(parseLocation());
		else
		{
			std::string value;
			if (directive == "server_name" || directive == "error_page")
				value = getRestOfLine();
			else
				value = getNextToken();
			parseServerDirective(server, directive, value);
			skipWhitespace();
			if (!isAtEnd() && content[pos] == ';')
				pos++;
		}
	}
	return server;
}

Route ConfigParser::parseLocation()
{
	Route route;
	route.path = getNextToken();

	if (route.path.empty())
		throwError("Location path cannot be empty");

	skipWhitespace();
	if (isAtEnd() || content[pos] != '{')
		throwError("Expected '{' after location path");

	pos++;
	while (!isAtEnd())
	{
		skipWhitespace();
		skipComments();
		skipWhitespace();
		if (isAtEnd())
			throwError("Unexpected end of file, expected '}'");

		if (content[pos] == '}')
		{
			pos++;
			break;
		}
		std::string directive = getNextToken();
		if (directive.empty())
			throwError("Empty directive in location block");

		std::string value;
		if (directive == "allow_methods" || directive == "index" || directive == "return")
			value = getRestOfLine();
		else
			value = getNextToken();
		parseLocationDirective(route, directive, value);
		skipWhitespace();
		if (!isAtEnd() && content[pos] == ';')
			pos++;
	}
	return route;
}

void ConfigParser::parseServerDirective(ServerConfig &server, const std::string &directive, const std::string &value)
{
	if (directive == "listen")
	{
		size_t colon_pos = value.find(':');
		if (colon_pos != std::string::npos)
		{
			server.host = value.substr(0, colon_pos);
			server.port = std::atoi(value.substr(colon_pos + 1).c_str());
		}
		else
			server.port = std::atoi(value.c_str());
	}
	else if (directive == "server_name")
	{
		server.server_names = split(value, ' ');
	}
	else if (directive == "error_page")
	{
		std::vector<std::string> parts = split(value, ' ');
		if (parts.size() >= 2)
		{
			std::string page_path = parts[parts.size() - 1];
			for (size_t i = 0; i < parts.size() - 1; ++i)
			{
				int error_code = std::atoi(parts[i].c_str());
				if (error_code > 0)
					server.error_pages[error_code] = page_path;
			}
		}
	}
	else if (directive == "client_max_body_size")
	{
		server.client_max_body_size = parseSize(value);
	}
	else
		throwError("Unknown server directive: " + directive);
}

void ConfigParser::parseLocationDirective(Route &route, const std::string &directive, const std::string &value)
{
	if (directive == "allow_methods")
	{
		route.allowed_methods = split(value, ' ');
	}
	else if (directive == "return")
	{
		std::vector<std::string> parts = split(value, ' ');
		if (parts.size() == 2)
		{
			route.redirect_code = std::atoi(parts[0].c_str());
			route.redirect_url = parts[1];
		}
		else if (parts.size() == 1)
		{
			route.redirect_code = 302;
			route.redirect_url = parts[0];
		}
		else
			throwError("Invalid return directive format");
	}
	else if (directive == "root")
	{
		route.root = value;
	}
	else if (directive == "autoindex")
	{
		route.directory_listing = (value == "on");
	}
	else if (directive == "index")
	{
		std::vector<std::string> index_files = split(value, ' ');
		if (!index_files.empty())
		{
			route.index = index_files[0];
			route.index_files = index_files;
		}
	}
	else if (directive == "cgi_extension")
	{
		route.cgi_extension = value;
	}
	else if (directive == "cgi_path")
	{
		route.cgi_path = value;
	}
	else if (directive == "upload_path")
	{
		route.upload_path = value;
		route.upload_enabled = true;
	}
	else
		throwError("Unknown location directive: " + directive);
}

std::vector<std::string> ConfigParser::split(const std::string &str, char delimiter) const
{
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string item;

	while (std::getline(ss, item, delimiter))
	{
		item = trim(item);
		if (!item.empty())
			result.push_back(item);
	}
	return result;
}

std::string ConfigParser::trim(const std::string &str) const
{
	size_t start = str.find_first_not_of(" \t\n\r");
	if (start == std::string::npos)
		return "";

	size_t end = str.find_last_not_of(" \t\n\r");
	return str.substr(start, end - start + 1);
}

bool ConfigParser::isValidMethod(const std::string &method) const
{
	return method == "GET" || method == "POST" || method == "DELETE" ||
		   method == "PUT" || method == "HEAD" || method == "OPTIONS";
}

bool ConfigParser::isValidPort(int port) const
{
	return port > 0 && port <= 65535;
}

size_t ConfigParser::parseSize(const std::string &sizeStr) const
{
	if (sizeStr.empty())
		return 0;

	std::string numStr = sizeStr;
	size_t multiplier = 1;
	char lastChar = std::tolower(sizeStr[sizeStr.length() - 1]);
	if (lastChar == 'k')
	{
		multiplier = 1024;
		numStr = sizeStr.substr(0, sizeStr.length() - 1);
	}
	else if (lastChar == 'm')
	{
		multiplier = 1024 * 1024;
		numStr = sizeStr.substr(0, sizeStr.length() - 1);
	}
	else if (lastChar == 'g')
	{
		multiplier = 1024 * 1024 * 1024;
		numStr = sizeStr.substr(0, sizeStr.length() - 1);
	}
	return std::atoi(numStr.c_str()) * multiplier;
}

void ConfigParser::validateConfig(const Config &config) const
{
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		validateServer(config.servers[i]);
	}
}

void ConfigParser::validateServer(const ServerConfig &server) const
{
	if (!isValidPort(server.port))
		throwError("Invalid port number");

	for (size_t i = 0; i < server.routes.size(); ++i)
	{
		validateRoute(server.routes[i]);
	}
}

void ConfigParser::validateRoute(const Route &route) const
{
	if (route.path.empty())
		throwError("Route path cannot be empty");

	for (size_t i = 0; i < route.allowed_methods.size(); ++i)
	{
		if (!isValidMethod(route.allowed_methods[i]))
			throwError("Invalid HTTP method: " + route.allowed_methods[i]);
	}
}

void ConfigParser::printConfig(const Config &config) const
{
	std::cout << "=== Configuration ===\n";
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		const ServerConfig &server = config.servers[i];
		std::cout << "\nServer " << (i + 1) << ":\n";
		std::cout << "  Host: " << server.host << "\n";
		std::cout << "  Port: " << server.port << "\n";
		std::cout << "  Max Body Size: " << server.client_max_body_size << " bytes\n";
		if (!server.server_names.empty())
		{
			std::cout << "  Server Names: ";
			for (size_t j = 0; j < server.server_names.size(); ++j)
			{
				std::cout << server.server_names[j];
				if (j < server.server_names.size() - 1)
					std::cout << ", ";
			}
			std::cout << "\n";
		}
		if (!server.error_pages.empty())
		{
			std::cout << "  Error Pages:\n";
			for (std::map<int, std::string>::const_iterator it = server.error_pages.begin();
				 it != server.error_pages.end(); ++it)
			{
				std::cout << "    " << it->first << " -> " << it->second << "\n";
			}
		}
		std::cout << "  Routes:\n";
		for (size_t j = 0; j < server.routes.size(); ++j)
		{
			const Route &route = server.routes[j];
			std::cout << "    Location: " << route.path << "\n";
			if (!route.allowed_methods.empty())
			{
				std::cout << "      Methods: ";
				for (size_t k = 0; k < route.allowed_methods.size(); ++k)
				{
					std::cout << route.allowed_methods[k];
					if (k < route.allowed_methods.size() - 1)
						std::cout << ", ";
				}
				std::cout << "\n";
			}
			if (!route.root.empty())
			{
				std::cout << "      Root: " << route.root << "\n";
			}
			if (!route.index.empty())
			{
				std::cout << "      Main Index File: " << route.index << "\n";
			}
			if (!route.index_files.empty())
			{
				std::cout << "      Index Files: ";
				for (size_t k = 0; k < route.index_files.size(); ++k)
				{
					std::cout << route.index_files[k];
					if (k < route.index_files.size() - 1)
						std::cout << ", ";
				}
				std::cout << "\n";
			}
			if (route.directory_listing)
			{
				std::cout << "      Directory Listing: ON\n";
			}
			if (!route.redirect_url.empty())
			{
				std::cout << "      Redirect: " << route.redirect_code << " " << route.redirect_url << "\n";
			}
			if (!route.cgi_extension.empty())
			{
				std::cout << "      CGI Extension: " << route.cgi_extension << "\n";
			}
			if (!route.cgi_path.empty())
			{
				std::cout << "      CGI Path: " << route.cgi_path << "\n";
			}
			if (route.upload_enabled)
			{
				std::cout << "      Upload Path: " << route.upload_path << "\n";
			}
		}
	}
	std::cout << "=== End of Configuration ===\n";
}
