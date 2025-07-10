#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

// Forward declarations
struct Route;
struct ServerConfig;
class ConfigParser;

// Route configuration structure
struct Route
{
	std::string path;
	std::vector<std::string> allowed_methods;
	int redirect_code;
	std::string redirect_url;
	std::string redirect;
	std::string root;
	bool directory_listing;
	std::vector<std::string> index_files;
	std::string cgi_extension;
	std::string cgi_path;
	std::string upload_path;
	bool upload_enabled;

	Route();
};

// Server configuration structure
struct ServerConfig
{
	std::string host;
	int port;
	std::vector<std::string> server_names;
	std::map<int, std::string> error_pages;
	size_t client_max_body_size;
	std::vector<Route> routes;

	ServerConfig();
	const Route *findMatchingRoute(const std::string &requestPath) const;
};

// Main configuration structure
struct Config
{
	std::vector<ServerConfig> servers;

	Config();
};

class ConfigParser
{
private:
	std::string content;
	size_t pos;
	size_t line_num;

	// State for parsing server directives
	struct ServerParseState
	{
		bool listen_found;
		bool client_max_body_size_found;

		ServerParseState();
	};

	// State for parsing location directives
	struct LocationParseState
	{
		bool root_found;
		bool return_found;
		bool autoindex_found;
		bool index_found;
		bool cgi_extension_found;
		bool cgi_path_found;
		bool upload_path_found;

		LocationParseState();
	};

	// Helper methods
	void skipWhitespace();
	void skipComments();
	std::string getNextToken();
	std::string getRestOfLine();
	bool isAtEnd() const;
	void throwError(const std::string &message) const;

	// Parsing methods
	Config parseConfig();
	ServerConfig parseServer();
	Route parseLocation();
	void parseServerDirective(ServerConfig &server, const std::string &directive, const std::string &value, ServerParseState &state);
	void parseLocationDirective(Route &route, const std::string &directive, const std::string &value, LocationParseState &state);

	// Validation methods
	void validateConfig(const Config &config) const;
	void validateServer(const ServerConfig &server) const;
	void validateRoute(const Route &route) const;

	// Utility methods
	std::vector<std::string> split(const std::string &str, char delimiter) const;
	std::string trim(const std::string &str) const;
	bool isValidMethod(const std::string &method) const;
	bool isValidPort(int port) const;
	size_t parseSize(const std::string &sizeStr) const;

public:
	ConfigParser();
	~ConfigParser();

	Config parseFile(const std::string &filename);
	Config parseString(const std::string &config_content);
	void printConfig(const Config &config) const;
};

#endif
