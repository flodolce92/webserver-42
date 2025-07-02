#ifndef FILE_SERVER_HPP
#define FILE_SERVER_HPP

#include <ConfigParser.hpp>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <map>
#include <fstream>

enum PathType
{
	STATIC_FILE,
	DIRECTORY,
	ERROR
};

struct ResolutionResult
{
	std::string path;
	PathType pathType;
	int statusCode;
};

class FileServer
{
private:
	FileServer();
	~FileServer();

	// Private helper to get stat information of a file/directory.
	static bool getStat(const std::string &path, struct stat &st);

	// Private helper for handling MIME types.
	static std::map<std::string, std::string> mimeTypes;
	static void initMimeTypes();

public:
	// Resolves the actual file path on the filesystem for a given request and location.
	// Returns the full file path if found (regular file).
	// Returns the directory path if it is a directory and listing is allowed (or if an index is requested).
	// Returns an empty string in case of file not found, access denied, or error.
	static ResolutionResult resolveStaticFilePath(const std::string &requestPath, const Location &location);

	// Reads the content of a file from the specified path and returns it as a string.
	// Returns an empty string in case of read error.
	static std::string readFileContent(const std::string &filePath);

	// Determines the MIME type of a file based on its extension.
	// Useful for setting the Content-Type header of the HTTP response.
	static std::string getMimeType(const std::string &filePath);

	// Generates an HTML string for the listing of a directory.
	// Should be called only if resolveStaticFilePath returned a directory path and directory_listing is enabled.
	static std::string generateDirectoryListing(const std::string &directoryPath);
};

#endif
