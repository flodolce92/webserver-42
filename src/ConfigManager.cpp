#include <ConfigManager.hpp>
#include <iostream>
#include <stdexcept>
#include <algorithm>
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
			bool isExactMatch = (normCurrRoutePath == normRequestPath);
			bool isDirectoryMatch = (normRequestPath.length() > normCurrRoutePath.length() &&
									 normRequestPath[normCurrRoutePath.length()] == '/');
			bool isRootMatch = (normCurrRoutePath == "/" && normRequestPath.rfind("/", 0) == 0);

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
	try
	{
		config = parser.parseFile(filename);
		config_file_path = filename;
		is_loaded = true;
		std::cout << "Configuration loaded successfully from: " << filename << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to load configuration: " << e.what() << std::endl;
		throw;
	}
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

	// Second pass: port match with any host
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

	// First, find servers matching host:port
	std::vector<const ServerConfig *> matching_servers;
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		const ServerConfig &server = config.servers[i];
		if (server.host == host && server.port == port)
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

	// Now find server with matching server_name
	for (size_t i = 0; i < matching_servers.size(); ++i)
	{
		const ServerConfig *server = matching_servers[i];
		for (size_t j = 0; j < server->server_names.size(); ++j)
		{
			if (server->server_names[j] == server_name)
				return server;
		}
	}

	return matching_servers.empty() ? NULL : matching_servers[0];
}

bool ConfigManager::isMethodAllowed(const Route &route, const std::string &method) const
{
	if (route.allowed_methods.empty())
		return true;

	for (size_t i = 0; i < route.allowed_methods.size(); ++i)
	{
		if (route.allowed_methods[i] == method)
			return true;
	}
	return false;
}

std::string ConfigManager::resolveFilePath(const Route &route, const std::string &request_path) const
{
	std::string normalized_request = normalizePath(request_path);
	std::string normalized_route = normalizePath(route.path);

	// Remove route prefix from request path
	std::string relative_path;
	if (normalized_request.find(normalized_route) == 0)
	{
		relative_path = normalized_request.substr(normalized_route.length());
		if (!relative_path.empty() && relative_path[0] == '/')
			relative_path = relative_path.substr(1);
	}

	// Combine with root directory
	std::string full_path = route.root;
	if (!full_path.empty() && full_path[full_path.length() - 1] != '/')
		full_path += '/';
	full_path += relative_path;

	return full_path;
}

bool ConfigManager::isCGIRequest(const Route &route, const std::string &file_path) const
{
	if (route.cgi_extension.empty())
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

void ConfigManager::validateConfiguration() const
{
	if (!is_loaded)
		throw std::runtime_error("Configuration not loaded");

	// Additional validation
	for (size_t i = 0; i < config.servers.size(); ++i)
	{
		const ServerConfig &server = config.servers[i];

		// Check for duplicate server blocks
		for (size_t j = i + 1; j < config.servers.size(); ++j)
		{
			const ServerConfig &other = config.servers[j];
			if (server.host == other.host && server.port == other.port)
				std::cerr << "Warning: Duplicate server block for " << server.host << ":" << server.port << std::endl;
		}

		// Validate routes
		for (size_t j = 0; j < server.routes.size(); ++j)
		{
			const Route &route = server.routes[j];

			// Check if CGI path exists when CGI is configured
			if (!route.cgi_extension.empty() && !route.cgi_path.empty())
			{
				struct stat st;
				if (stat(route.cgi_path.c_str(), &st) != 0)
					std::cerr << "Warning: CGI path does not exist: " << route.cgi_path << std::endl;
			}
		}
	}
}

// Utility function to normalize paths
std::string normalizePath(const std::string &path)
{
	if (path.empty())
		return "/";

	std::string normalized = path;
	if (normalized[0] != '/')
		normalized = "/" + normalized;

	// Remove trailing slash except for root
	if (normalized.length() > 1 && normalized[normalized.length() - 1] == '/')
		normalized = normalized.substr(0, normalized.length() - 1);

	return normalized;
}
