#ifndef CONFIGMANAGER_HPP
#define CONFIGMANAGER_HPP

#include <ConfigParser.hpp>
#include <string>
#include <vector>
#include <map>

class ConfigManager
{
private:
	Config config;
	ConfigParser parser;
	bool is_loaded;
	std::string config_file_path;

public:
	ConfigManager();
	~ConfigManager();

	// Configuration loading
	void loadConfig(const std::string &filename);
	bool isLoaded() const;

	// Server access methods
	const std::vector<ServerConfig> &getServers() const;
	const ServerConfig *findServer(const std::string &host, int port) const;
	const ServerConfig *findServerByName(const std::string &server_name, const std::string &host, int port) const;

	// Utility methods
	bool isMethodAllowed(const Location &location, const std::string &method) const;
	std::string resolveFilePath(const Location &location, const std::string &request_path) const;
	bool isCGIRequest(const Location &location, const std::string &file_path) const;

	// Debug methods
	void printConfiguration() const;
};

std::string normalizePath(const std::string &path);

#endif
