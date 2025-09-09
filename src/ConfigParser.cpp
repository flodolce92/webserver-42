#include <ConfigParser.hpp>
#include <cctype>
#include <sys/stat.h>

// Location
Location::Location()
	: path(""),
	  allowed_methods(),
	  redirect_code(0),
	  redirect_url(""),
	  redirect(""),
	  root(""),
	  autoindex(false),
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
	  locations(),
	  root(""),
	  index_files(),
	  autoindex(false),
	  allowed_methods()
{
}

// ServerParseState
ConfigParser::ServerParseState::ServerParseState()
	: listen_found(false),
	  client_max_body_size_found(false),
	  root_found(false),
	  index_found(false),
	  autoindex_found(false)
{
}

// LocationParseState
ConfigParser::LocationParseState::LocationParseState()
	: root_found(false),
	  return_found(false),
	  autoindex_found(false),
	  index_found(false),
	  cgi_extension_found(false),
	  cgi_path_found(false),
	  upload_path_found(false)
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
	ServerParseState state;

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
			server.locations.push_back(parseLocation(server));
		else
		{
			std::string value;
			if (directive == "server_name" || directive == "error_page" ||
				directive == "allow_methods" || directive == "index")
				value = getRestOfLine();
			else
				value = getNextToken();

			if (value.empty())
				throwError("Missing value for directive: " + directive);

			parseServerDirective(server, directive, value, state);

			skipWhitespace();
			if (!isAtEnd() && content[pos] == ';')
				pos++;
			else if (!isAtEnd() && content[pos] != '{')
				throwError("Expected ';' or '{' after directive: " + directive);
		}
	}

	// Check for mandatory directives
	if (!state.listen_found)
		throwError("Mandatory 'listen' directive is missing in server block");

	return server;
}

Location ConfigParser::parseLocation(const ServerConfig &server)
{
	Location location;
	LocationParseState state;

	// Inherit server directives
	location.root = server.root;
	location.index_files = server.index_files;
	location.autoindex = server.autoindex;
	location.allowed_methods = server.allowed_methods;

	location.path = getNextToken();

	if (location.path.empty() || location.path[0] != '/')
		throwError("Location path must start with '/'");

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

		if (value.empty())
			throwError("Missing value for directive: " + directive);

		parseLocationDirective(location, directive, value, state);

		skipWhitespace();
		if (!isAtEnd() && content[pos] == ';')
			pos++;
		else if (!isAtEnd() && content[pos] != '{')
			throwError("Expected ';' or '{' after directive: " + directive);
	}

	// Logical validation after parsing the block
	if (location.root.empty() && !state.return_found && !state.cgi_path_found)
		throwError("Location block for path '" + location.path + "' must have a 'root', 'return', or 'cgi_path' directive.");

	if (state.cgi_extension_found && !state.cgi_path_found)
		throwError("'cgi_extension' is set without a 'cgi_path' in location '" + location.path + "'");

	if (state.cgi_path_found && !state.cgi_extension_found)
		throwError("'cgi_path' is set without a 'cgi_extension' in location '" + location.path + "'");

	if (state.upload_path_found)
	{
		std::vector<std::string> methods = location.allowed_methods;
		bool post_found = false;
		for (size_t i = 0; i < methods.size(); ++i)
		{
			if (methods[i] == "POST")
			{
				post_found = true;
				break;
			}
		}
		if (!post_found)
			throwError("'upload_path' is set, but 'POST' method is not specified in 'allow_methods' for location '" + location.path + "'");
	}

	if (state.return_found)
	{
		if (location.redirect_code < 300 || location.redirect_code >= 400)
			throwError("Invalid redirect code in 'return' directive for location '" + location.path + "'. Must be a 3xx code.");
		if (location.redirect_url.empty())
			throwError("'return' directive must specify a URL for location '" + location.path + "'");

		location.root.clear();
		location.index_files.clear();
		location.autoindex = false;
	}

	return location;
}

void ConfigParser::parseServerDirective(ServerConfig &server, const std::string &directive, const std::string &value, ServerParseState &state)
{
	if (directive == "listen")
	{
		if (state.listen_found)
			throwError("Duplicate 'listen' directive");
		state.listen_found = true;
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
		else
			throwError("Invalid 'error_page' format. Expected at least one code and a path.");
	}
	else if (directive == "client_max_body_size")
	{
		if (state.client_max_body_size_found)
			throwError("Duplicate 'client_max_body_size' directive");
		state.client_max_body_size_found = true;
		server.client_max_body_size = parseSize(value);
	}
	else if (directive == "root")
	{
		if (state.root_found)
			throwError("Duplicate 'root' directive");
		state.root_found = true;
		server.root = value;
	}
	else if (directive == "index")
	{
		if (state.index_found)
			throwError("Duplicate 'index' directive");
		state.index_found = true;
		server.index_files = split(value, ' ');
		if (server.index_files.empty())
			throwError("'index' directive must specify at least one file");
	}
	else if (directive == "autoindex")
	{
		if (state.autoindex_found)
			throwError("Duplicate 'autoindex' directive");
		state.autoindex_found = true;
		server.autoindex = (value == "on");
	}
	else if (directive == "allow_methods")
	{
		server.allowed_methods = split(value, ' ');
		for (size_t i = 0; i < server.allowed_methods.size(); ++i)
		{
			if (!isValidMethod(server.allowed_methods[i]))
				throwError("Invalid HTTP method: " + server.allowed_methods[i]);
		}
	}
	else
		throwError("Unknown server directive: " + directive);
}

void ConfigParser::parseLocationDirective(Location &location, const std::string &directive, const std::string &value, LocationParseState &state)
{
	if (directive == "allow_methods")
	{
		location.allowed_methods = split(value, ' ');
		for (size_t i = 0; i < location.allowed_methods.size(); ++i)
		{
			if (!isValidMethod(location.allowed_methods[i]))
				throwError("Invalid HTTP method: " + location.allowed_methods[i]);
		}
	}
	else if (directive == "return")
	{
		if (state.return_found)
			throwError("Duplicate 'return' directive");
		if (state.root_found)
			throwError("'return' directive conflicts with 'root'");
		state.return_found = true;

		std::vector<std::string> parts = split(value, ' ');
		if (parts.size() == 2)
		{
			location.redirect_code = std::atoi(parts[0].c_str());
			location.redirect_url = parts[1];
		}
		else if (parts.size() == 1)
		{
			location.redirect_code = 302; // Default redirect
			if (parts[0].find("http://") == 0 || parts[0].find("https://") == 0 ||
				parts[0].find("/") == 0)
			{
				location.redirect_url = parts[0];
			}
			else
				throwError("Invalid return directive format, expected 'code url' or 'url'");
		}
		else
			throwError("Invalid return directive format");
	}
	else if (directive == "root")
	{
		if (state.root_found)
			throwError("Duplicate 'root' directive");
		if (state.return_found)
			throwError("'root' directive conflicts with 'return'");
		state.root_found = true;
		location.root = value;
	}
	else if (directive == "autoindex")
	{
		if (state.autoindex_found)
			throwError("Duplicate 'autoindex' directive");
		state.autoindex_found = true;
		location.autoindex = (value == "on");
	}
	else if (directive == "index")
	{
		if (state.index_found)
			throwError("Duplicate 'index' directive");
		state.index_found = true;
		location.index_files = split(value, ' ');
		if (location.index_files.empty())
			throwError("'index' directive must specify at least one file");
	}
	else if (directive == "cgi_extension")
	{
		if (state.cgi_extension_found)
			throwError("Duplicate 'cgi_extension' directive");
		state.cgi_extension_found = true;
		location.cgi_extension = value;
	}
	else if (directive == "cgi_path")
	{
		if (state.cgi_path_found)
			throwError("Duplicate 'cgi_path' directive");
		state.cgi_path_found = true;
		location.cgi_path = value;
	}
	else if (directive == "upload_path")
	{
		if (state.upload_path_found)
			throwError("Duplicate 'upload_path' directive");
		state.upload_path_found = true;
		location.upload_path = value;
		location.upload_enabled = true;
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

	for (size_t i = 0; i < numStr.length(); ++i)
	{
		if (!std::isdigit(numStr[i]))
			throwError("Invalid size format: " + sizeStr);
	}

	long long value = std::atoll(numStr.c_str());
	if (value <= 0)
		throwError("Size must be a positive value: " + sizeStr);

	return value * multiplier;
}

void ConfigParser::validateConfig(const Config &config) const
{
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		// Check for duplicate server blocks (same host and port)
		for (size_t j = i + 1; j < config.servers.size(); ++j)
		{
			const ServerConfig &other = config.servers[j];
			if (config.servers[i].host == other.host && config.servers[i].port == other.port)
			{
				std::ostringstream oss;
				oss << "Duplicate server block for " << config.servers[i].host << ":" << config.servers[i].port;
				throwError(oss.str());
			}
		}
		validateServer(config.servers[i]);
	}
}

void ConfigParser::validateServer(const ServerConfig &server) const
{
	if (!isValidPort(server.port))
		throwError("Invalid port number");

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		validateLocation(server.locations[i]);
	}
}

void ConfigParser::validateLocation(const Location &location) const
{
	if (location.path.empty())
		throwError("Location path cannot be empty");

	for (size_t i = 0; i < location.allowed_methods.size(); ++i)
	{
		if (!isValidMethod(location.allowed_methods[i]))
			throwError("Invalid HTTP method: " + location.allowed_methods[i]);
	}

	if (!location.cgi_path.empty())
	{
		struct stat st;
		if (stat(location.cgi_path.c_str(), &st) != 0)
		{
			throwError("CGI path does not exist: " + location.cgi_path);
		}
		if (!(st.st_mode & S_IXUSR))
		{
			throwError("CGI path is not executable: " + location.cgi_path);
		}
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
		std::cout << "  Locations:\n";
		for (size_t j = 0; j < server.locations.size(); ++j)
		{
			const Location &location = server.locations[j];
			std::cout << "    Location: " << location.path << "\n";
			if (!location.allowed_methods.empty())
			{
				std::cout << "      Methods: ";
				for (size_t k = 0; k < location.allowed_methods.size(); ++k)
				{
					std::cout << location.allowed_methods[k];
					if (k < location.allowed_methods.size() - 1)
						std::cout << ", ";
				}
				std::cout << "\n";
			}
			if (!location.root.empty())
			{
				std::cout << "      Root: " << location.root << "\n";
			}
			if (!location.index_files.empty())
			{
				std::cout << "      Index Files: ";
				for (size_t k = 0; k < location.index_files.size(); ++k)
				{
					std::cout << location.index_files[k];
					if (k < location.index_files.size() - 1)
						std::cout << ", ";
				}
				std::cout << "\n";
			}
			if (location.autoindex)
			{
				std::cout << "      Autoindex: ON\n";
			}
			if (!location.redirect_url.empty())
			{
				std::cout << "      Redirect: " << location.redirect_code << " " << location.redirect_url << "\n";
			}
			if (!location.cgi_extension.empty())
			{
				std::cout << "      CGI Extension: " << location.cgi_extension << "\n";
			}
			if (!location.cgi_path.empty())
			{
				std::cout << "      CGI Path: " << location.cgi_path << "\n";
			}
			if (location.upload_enabled)
			{
				std::cout << "      Upload Path: " << location.upload_path << "\n";
			}
		}
	}
	std::cout << "=== End of Configuration ===\n";
}
