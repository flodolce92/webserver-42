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
	std::string listing = "<html><head><title>Index of " + directoryPath + "</title>";
	listing += "<style>";
	listing += "body { font-family: monospace; background-color: #f0f0f0; color: #333; }";
	listing += "h1 { color: #0056b3; }";
	listing += "pre { background-color: #fff; padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); overflow-x: auto; }";
	listing += "a { color: #007bff; text-decoration: none; }";
	listing += "a:hover { text-decoration: underline; }";
	listing += "</style>";
	listing += "</head><body>";
	listing += "<h1>Index of " + directoryPath + "</h1><hr><pre>";

	DIR *dir = opendir(directoryPath.c_str());
	if (!dir)
	{
		// std::cerr << "FileServer::generateDirectoryListing: Could not open directory: " << directoryPath << std::endl;
		return "<h1>Error: Could not list directory contents.</h1></body></html>";
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..")
		{
			if (name == "..")
				listing += "<a href=\"../\">../</a>\n";
			continue;
		}

		std::string fullEntryPath = directoryPath;
		if (fullEntryPath[fullEntryPath.length() - 1] != '/')
			fullEntryPath += '/';
		fullEntryPath += name;

		struct stat st;
		if (stat(fullEntryPath.c_str(), &st) == 0)
		{
			if (S_ISDIR(st.st_mode))
				name += "/"; // Append slash for directories
			// Generate link relative to the current directory
			listing += "<a href=\"" + name + "\">" + name + "</a>\n";
		}
		else
		{
			// Fallback if stat fails for some reason (e.g., permissions)
			listing += name + "\n";
		}
	}
	closedir(dir);

	listing += "</pre><hr></body></html>";
	return listing;
}
