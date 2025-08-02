#include <ConfigManager.hpp>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

const Location *ServerConfig::findMatchingLocation(const std::string &requestPath) const
{
	const Location *bestMatch = NULL;
	size_t longestMatchLength = 0;

	for (size_t i = 0; i < locations.size(); ++i)
	{
		const Location &currentLocation = locations[i];
		std::string normRequestPath = normalizePath(requestPath);
		std::string normCurrLocationPath = normalizePath(currentLocation.path);

		if (normRequestPath.rfind(normCurrLocationPath, 0) == 0)
		{
			bool isExactMatch = (normCurrLocationPath.length() == normRequestPath.length());
			bool isDirectoryMatch = !isExactMatch &&
									(normRequestPath.length() > normCurrLocationPath.length() &&
									 normRequestPath[normCurrLocationPath.length()] == '/');
			bool isRootMatch = (normCurrLocationPath == "/");

			if (isExactMatch || isDirectoryMatch || isRootMatch)
			{
				if (normCurrLocationPath.length() > longestMatchLength)
				{
					longestMatchLength = normCurrLocationPath.length();
					bestMatch = &currentLocation;
				}
			}
		}
	}
	return bestMatch;
}

ConfigManager::ConfigManager() : is_loaded(false), config_file_path("") {}

ConfigManager::~ConfigManager() {}

void ConfigManager::loadConfig(const std::string &filename)
{
	config = parser.parseFile(filename);
	config_file_path = filename;
	is_loaded = true;
	std::cout << "Configuration loaded and validated successfully from: " << filename << std::endl;
}

bool ConfigManager::isLoaded() const
{
	return is_loaded;
}

const std::vector<ServerConfig> &ConfigManager::getServers() const
{
	if (!is_loaded)
		throw std::runtime_error("Configuration not loaded");
	return config.servers;
}

const ServerConfig *ConfigManager::findServer(const std::string &host, int port) const
{
	if (!is_loaded)
		return NULL;

	// First pass: exact host and port match
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		const ServerConfig &server = config.servers[i];
		if (server.host == host && server.port == port)
			return &server;
	}

	// Second pass: port match with any host ("0.0.0.0" or wildcard)
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		const ServerConfig &server = config.servers[i];
		if (server.port == port && (server.host == "0.0.0.0" || server.host == "*"))
			return &server;
	}

	// Fallback to first server on that port (default)
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		const ServerConfig &server = config.servers[i];
		if (server.port == port)
			return &server;
	}

	return NULL;
}

const ServerConfig *ConfigManager::findServerByName(const std::string &server_name, const std::string &host, int port) const
{
	if (!is_loaded)
		return NULL;

	if (server_name.empty())
		return findServer(host, port);

	// First, find servers matching host:port
	std::vector<const ServerConfig *> matching_servers;
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		const ServerConfig &server = config.servers[i];
		if ((server.host == host || server.host == "0.0.0.0" || server.host == "*") && server.port == port)
			matching_servers.push_back(&server);
	}

	// If no exact host:port match, try port only
	if (matching_servers.empty())
	{
		for (size_t i = 0; i < config.servers.size(); ++i)
		{
			const ServerConfig &server = config.servers[i];
			if (server.port == port)
				matching_servers.push_back(&server);
		}
	}

	if (matching_servers.empty())
		return NULL;

	// Now find server with matching server_name from the candidates
	for (size_t i = 0; i < matching_servers.size(); ++i)
	{
		const ServerConfig *server = matching_servers[i];
		for (size_t j = 0; j < server->server_names.size(); ++j)
		{
			if (server->server_names[j] == server_name)
				return server;
		}
	}

	// If no name matches, return the first server from the candidates as default
	return matching_servers[0];
}

bool ConfigManager::isMethodAllowed(const Location &location, const std::string &method) const
{
	// If allow_methods is empty, all mandatory methods are allowed.
	if (location.allowed_methods.empty())
		return (method == "GET" || method == "POST" || method == "DELETE");

	for (size_t i = 0; i < location.allowed_methods.size(); ++i)
	{
		if (location.allowed_methods[i] == method)
			return true;
	}
	return false;
}

std::string ConfigManager::resolveFilePath(const Location &location, const std::string &request_path) const
{
	// Ensure root path ends with a slash for correct concatenation
	std::string root_path = location.root;
	if (!root_path.empty() && root_path[root_path.length() - 1] != '/')
		root_path += '/';

	// Ensure location path starts with a slash
	std::string location_path = normalizePath(location.path);

	// Get the part of the request path that comes after the location's path
	std::string relative_path;
	if (request_path.find(location_path) == 0)
	{
		if (location_path == "/")
			relative_path = request_path.substr(1);
		else
			relative_path = request_path.substr(location_path.length());
	}

	// Remove leading slash from relative_path if it exists
	if (!relative_path.empty() && relative_path[0] == '/')
		relative_path = relative_path.substr(1);

	return root_path + relative_path;
}

bool ConfigManager::isCGIRequest(const Location &location, const std::string &file_path) const
{
	if (location.cgi_extension.empty() || location.cgi_path.empty())
		return false;

	size_t dot_pos = file_path.find_last_of('.');
	if (dot_pos == std::string::npos)
		return false;

	std::string extension = file_path.substr(dot_pos);
	return extension == location.cgi_extension;
}

void ConfigManager::printConfiguration() const
{
	if (!is_loaded)
	{
		std::cout << "No configuration loaded" << std::endl;
		return;
	}
	parser.printConfig(config);
}

// Utility function to normalize paths
std::string normalizePath(const std::string &path)
{
	if (path.empty())
		return "/";

	std::string normalized = path;

	// Remove trailing slash except for root
	if (normalized.length() > 1 && normalized[normalized.length() - 1] == '/')
		normalized = normalized.substr(0, normalized.length() - 1);

	return normalized;
}
