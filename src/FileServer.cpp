#include <FileServer.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>
#include <unistd.h>

std::map<std::string, std::string> FileServer::mimeTypes;

FileServer::FileServer() {}
FileServer::~FileServer() {}

std::string FileServer::normalizePath(const std::string &path)
{
	if (path.empty())
		return "/";

	std::string normalized = path;
	if (normalized[0] != '/')
		normalized = "/" + normalized;

	if (normalized.length() > 1 && normalized[normalized.length() - 1] == '/')
		normalized = normalized.substr(0, normalized.length() - 1);

	return normalized;
}

// Helper private method to get stat info
bool FileServer::getStat(const std::string &path, struct stat &st)
{
	if (stat(path.c_str(), &st) != 0)
	{
		// std::cerr << "FileServer::getStat error: " << strerror(errno) << " for path: " << path << std::endl;
		return false;
	}
	return true;
}

ResolutionResult FileServer::resolveStaticFilePath(const std::string &requestPath, const Location &location)
{
	ResolutionResult result;
	std::string full_fs_path = location.root;
	std::string normalizedRequestPath = normalizePath(requestPath);

	if (location.root.length() > 1 || normalizedRequestPath != "/")
	{
		if (location.root[location.root.length() - 1] == '/' && normalizedRequestPath[0] == '/')
			full_fs_path += normalizedRequestPath.substr(1);
		else
			full_fs_path += normalizedRequestPath;
	}

	struct stat st;
	if (!getStat(full_fs_path, st))
	{
		result.path = "";
		result.pathType = ERROR;
		result.statusCode = 404;
		return result; // Path not found
	}

	if (access(full_fs_path.c_str(), R_OK) != 0)
	{
		result.path = "";
		result.pathType = ERROR;
		result.statusCode = 403;
		return result; // Forbidden
	}

	if (S_ISREG(st.st_mode))
	{
		result.path = full_fs_path;
		result.pathType = STATIC_FILE;
		result.statusCode = 200;
		return result; // Found a regular file
	}
	else if (S_ISDIR(st.st_mode))
	{
		// It's a directory, try to find an index file
		if (full_fs_path[full_fs_path.length() - 1] != '/')
			full_fs_path += '/';

		// Iterate through index_files
		for (size_t i = 0; i < location.index_files.size(); ++i)
		{
			std::string indexPath = full_fs_path + location.index_files[i];
			struct stat index_st;
			if (getStat(indexPath, index_st) &&
				S_ISREG(index_st.st_mode) &&
				access(indexPath.c_str(), R_OK) == 0)
			{
				result.path = indexPath;
				result.pathType = STATIC_FILE;
				result.statusCode = 200;
				return result; // Found an index file from the list
			}
		}

		// If no index file found for a directory:
		if (location.autoindex)
		{
			// Return the directory path itself, the caller will generate listing.
			result.path = full_fs_path;
			result.pathType = DIRECTORY;
			result.statusCode = 200;
		}
		else
		{
			// No index, no directory listing allowed (e.g., 403 Forbidden)
			result.path = "";
			result.pathType = ERROR;
			result.statusCode = 403;
		}
	}
	return result;
}

std::string FileServer::readFileContent(const std::string &filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
	{
		// std::cerr << "FileServer::readFileContent: Could not open file: " << filePath << std::endl;
		return "";
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	file.close();
	return buffer.str();
}

bool FileServer::saveFile(const std::string &filePath, const std::string &fileContent)
{
	// Check if the directory exists and is writable
	std::string directoryPath = filePath.substr(0, filePath.find_last_of('/'));
	struct stat st;
	if (stat(directoryPath.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
	{
		return false; // Directory does not exist or is not a directory
	}
	if (access(directoryPath.c_str(), W_OK) != 0)
	{
		return false; // Directory not writable
	}

	std::ofstream file(filePath.c_str(), std::ios::binary);
	if (!file.is_open())
	{
		return false;
	}

	file.write(fileContent.c_str(), fileContent.length());
	if (file.fail())
	{
		file.close();
		return false;
	}

	file.close();
	return true;
}

void FileServer::initMimeTypes()
{
	if (!mimeTypes.empty())
		return;

	mimeTypes[".html"] = "text/html";
	mimeTypes[".htm"] = "text/html";
	mimeTypes[".css"] = "text/css";
	mimeTypes[".js"] = "application/javascript";
	mimeTypes[".json"] = "application/json";
	mimeTypes[".jpg"] = "image/jpeg";
	mimeTypes[".jpeg"] = "image/jpeg";
	mimeTypes[".png"] = "image/png";
	mimeTypes[".gif"] = "image/gif";
	mimeTypes[".ico"] = "image/x-icon";
	mimeTypes[".svg"] = "image/svg+xml";
	mimeTypes[".pdf"] = "application/pdf";
	mimeTypes[".xml"] = "application/xml";
	mimeTypes[".txt"] = "text/plain";
	mimeTypes[".mp4"] = "video/mp4";
	mimeTypes[".webm"] = "video/webm";
	mimeTypes[".ogg"] = "video/ogg";
	mimeTypes[".mp3"] = "audio/mpeg";
	mimeTypes[".wav"] = "audio/wav";
	mimeTypes[".zip"] = "application/zip";
	mimeTypes[".gz"] = "application/gzip";
	mimeTypes[".tar"] = "application/x-tar";
	mimeTypes[".php"] = "application/x-httpd-php";
	mimeTypes[".py"] = "text/x-python";
	// Default for unknown types
	mimeTypes["default"] = "application/octet-stream";
}

std::string FileServer::getMimeType(const std::string &filePath)
{
	if (mimeTypes.empty())
		initMimeTypes();

	size_t dotPos = filePath.rfind('.');
	if (dotPos == std::string::npos)
		return mimeTypes["default"];

	std::string extension = filePath.substr(dotPos);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	std::map<std::string, std::string>::const_iterator it = mimeTypes.find(extension);
	if (it != mimeTypes.end())
		return it->second;
	return mimeTypes["default"];
}

std::string FileServer::generateDirectoryListing(const std::string &directoryPath, const std::string &requestPath)
{
	std::string listing;
	listing += "<!DOCTYPE html><html><head><title>Index of " + requestPath + "</title>";
	listing += "<style>body { font-family: monospace; background-color: #f0f0f0; margin: 20px; }";
	listing += "h1 { color: #333; }";
	listing += "table { width: 100%; border-collapse: collapse; }";
	listing += "th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }";
	listing += ".container { background-color: #fff; padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); overflow-x: auto; }";
	listing += "a { color: #007bff; text-decoration: none; }";
	listing += "a:hover { text-decoration: underline; }";
	listing += "</style>";
	listing += "</head><body>";
	listing += "<h1>Index of " + requestPath + "</h1><hr><pre>";

	DIR *dir = opendir(directoryPath.c_str());
	if (!dir)
	{
		std::cerr << "FileServer::generateDirectoryListing: Could not open directory: " << directoryPath << std::endl;
		return "<h1>Error: Could not list directory contents.</h1></body></html>";
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;

		if (name == ".")
			continue;

		if (name == "..")
		{
			std::string parentPath = requestPath;
			size_t lastSlashPos = parentPath.find_last_of('/', parentPath.length() - 2);
			if (lastSlashPos != std::string::npos)
			{
				parentPath = parentPath.substr(0, lastSlashPos + 1);
				listing += "<a href=\"" + parentPath + "\">../</a>\n";
			}
			else
			{
				listing += "<a href=\"/\">../</a>\n";
			}
			continue;
		}

		std::string fullEntryPath = directoryPath;
		if (fullEntryPath[fullEntryPath.length() - 1] != '/')
			fullEntryPath += '/';
		fullEntryPath += name;

		struct stat st;
		if (stat(fullEntryPath.c_str(), &st) != 0)
			continue;

		std::string linkPath = requestPath;
		if (linkPath[linkPath.length() - 1] != '/')
			linkPath += '/';
		linkPath += name;

		if (S_ISDIR(st.st_mode))
			linkPath += '/';

		listing += "<a href=\"" + linkPath + "\">" + name;
		if (S_ISDIR(st.st_mode))
			listing += "/";
		listing += "</a>\n";
	}

	closedir(dir);
	listing += "</pre><hr></body></html>";

	return listing;
}
