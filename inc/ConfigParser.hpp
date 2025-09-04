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
struct Location;
struct ServerConfig;
struct ResolutionResult;
class ConfigParser;

// Location configuration structure
struct Location
{
	std::string path;
	std::vector<std::string> allowed_methods;
	int redirect_code;
	std::string redirect_url;
	std::string redirect;
	std::string root;
	bool autoindex;
	std::vector<std::string> index_files;
	std::string cgi_extension;
	std::string cgi_path;
	std::string upload_path;
	bool upload_enabled;

	Location();
};

// Server configuration structure
struct ServerConfig
{
	std::string host;
	int port;
	std::vector<std::string> server_names;
	std::map<int, std::string> error_pages;
	size_t client_max_body_size;
	std::vector<Location> locations;

	// Direttive ereditabili
	std::string root;
	std::vector<std::string> index_files;
	bool autoindex;
	std::vector<std::string> allowed_methods;

	ServerConfig();

	// Finds the best matching location for a given request path.
	// Returns a pointer to the best matching location, or nullptr if no match is found.
	const Location *findMatchingLocation(const std::string &requestPath) const;

	// Returns the error page for a given status code and location.
	// If no custom error page is found, empty path is returned,
	// with statusCode set to the provided code and pathType set to ERROR.
	ResolutionResult getErrorPage(int code, const Location &location) const;
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
		bool root_found;
		bool index_found;
		bool autoindex_found;

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
	Location parseLocation(const ServerConfig &server);
	void parseServerDirective(ServerConfig &server, const std::string &directive, const std::string &value, ServerParseState &state);
	void parseLocationDirective(Location &location, const std::string &directive, const std::string &value, LocationParseState &state);

	// Validation methods
	void validateConfig(const Config &config) const;
	void validateServer(const ServerConfig &server) const;
	void validateLocation(const Location &location) const;

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
