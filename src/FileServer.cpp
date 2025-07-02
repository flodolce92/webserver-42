#include <FileServer.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>

std::map<std::string, std::string> FileServer::mimeTypes;

FileServer::FileServer() {}
FileServer::~FileServer() {}

static std::string normalizePathInternal(const std::string &path)
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

std::string FileServer::resolveStaticFilePath(const std::string &requestPath, const Route &route)
{
	std::string full_fs_path = route.root;
	std::string normalizedRequestPath = normalizePathInternal(requestPath);

	if (route.root.length() > 1 || normalizedRequestPath != "/")
	{
		if (route.root[route.root.length() - 1] == '/' && normalizedRequestPath[0] == '/')
			full_fs_path += normalizedRequestPath.substr(1);
		else
			full_fs_path += normalizedRequestPath;
	}

	struct stat st;
	if (!getStat(full_fs_path, st))
	{
		return ""; // Path does not exist or cannot be accessed
	}

	if (S_ISREG(st.st_mode))
	{
		return full_fs_path;
	}
	else if (S_ISDIR(st.st_mode))
	{
		// It's a directory, try to find an index file

		if (full_fs_path[full_fs_path.length() - 1] != '/')
			full_fs_path += '/';

		// Iterate through index_files
		for (size_t i = 0; i < route.index_files.size(); ++i)
		{
			std::string indexPath = full_fs_path + route.index_files[i];
			struct stat index_st;
			if (getStat(indexPath, index_st) && S_ISREG(index_st.st_mode))
				return indexPath; // Found an index file from the list
		}

		// If no index file found for a directory:
		if (route.autoindex)
		{
			// Return the directory path itself, the caller will generate listing.
			return full_fs_path;
		}
		else
		{
			// No index, no directory listing allowed (e.g., 403 Forbidden)
			return "";
		}
	}
	return ""; // Not a regular file or directory, return empty
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

std::string FileServer::generateDirectoryListing(const std::string &directoryPath)
{
}
