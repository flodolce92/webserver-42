#include <ConfigManager.hpp>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

const Route *ServerConfig::findMatchingRoute(const std::string &requestPath) const
{
	const Route *bestMatch = NULL;
	size_t longestMatchLength = 0;

	for (size_t i = 0; i < routes.size(); ++i)
	{
		const Route &currentRoute = routes[i];
		std::string normRequestPath = normalizePath(requestPath);
		std::string normCurrRoutePath = normalizePath(currentRoute.path);

		if (normRequestPath.rfind(normCurrRoutePath, 0) == 0)
		{
			bool isExactMatch = (normCurrRoutePath.length() == normRequestPath.length());
			bool isDirectoryMatch = !isExactMatch &&
									(normRequestPath.length() > normCurrRoutePath.length() &&
									 normRequestPath[normCurrRoutePath.length()] == '/');
			bool isRootMatch = (normCurrRoutePath == "/");

			if (isExactMatch || isDirectoryMatch || isRootMatch)
			{
				if (normCurrRoutePath.length() > longestMatchLength)
				{
					longestMatchLength = normCurrRoutePath.length();
					bestMatch = &currentRoute;
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

bool ConfigManager::isMethodAllowed(const Route &route, const std::string &method) const
{
	// If allow_methods is empty, all mandatory methods are allowed.
	if (route.allowed_methods.empty())
		return (method == "GET" || method == "POST" || method == "DELETE");

	for (size_t i = 0; i < route.allowed_methods.size(); ++i)
	{
		if (route.allowed_methods[i] == method)
			return true;
	}
	return false;
}

std::string ConfigManager::resolveFilePath(const Route &route, const std::string &request_path) const
{
	// Ensure root path ends with a slash for correct concatenation
	std::string root_path = route.root;
	if (!root_path.empty() && root_path[root_path.length() - 1] != '/')
		root_path += '/';

	// Ensure route path starts with a slash
	std::string route_path = normalizePath(route.path);

	// Get the part of the request path that comes after the location's path
	std::string relative_path;
	if (request_path.find(route_path) == 0)
	{
		if (route_path == "/")
			relative_path = request_path.substr(1);
		else
			relative_path = request_path.substr(route_path.length());
	}

	// Remove leading slash from relative_path if it exists
	if (!relative_path.empty() && relative_path[0] == '/')
		relative_path = relative_path.substr(1);

	return root_path + relative_path;
}

bool ConfigManager::isCGIRequest(const Route &route, const std::string &file_path) const
{
	if (route.cgi_extension.empty() || route.cgi_path.empty())
		return false;

	size_t dot_pos = file_path.find_last_of('.');
	if (dot_pos == std::string::npos)
		return false;

	std::string extension = file_path.substr(dot_pos);
	return extension == route.cgi_extension;
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
