#include <ConfigManager.hpp>
#include <FileServer.hpp>
#include <Server.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
	if (argc == 2)
	{
		try
		{
			ConfigManager configManager;
			configManager.loadConfig(argv[1]);

			std::cout << "✅ Successfully parsed: " << argv[1] << "\n";
			configManager.printConfiguration();
			std::cout << std::endl;

			// Test location finder on first server
			const std::vector<ServerConfig> &servers = configManager.getServers();
			std::string requestPath = "/";
			if (!servers.empty())
			{
				const ServerConfig &server = servers[0];
				const Location *matchedLocation = server.findMatchingLocation(requestPath);
				if (matchedLocation)
				{
					std::cout << "✅ Matched location for path '" << requestPath << "':\n";
					std::cout << "Path: " << matchedLocation->path << "\n";
					std::cout << "Root: " << matchedLocation->root << "\n";

					// Resolve file path using FileServer
					ResolutionResult result = FileServer::resolveStaticFilePath(requestPath, *matchedLocation);
					if (!result.path.empty())
					{
						std::cout << "File path resolved: " << result.path << "\n";
						std::string content = FileServer::readFileContent(result.path);
						if (!content.empty())
							std::cout << "File content read successfully.\n";
					}
					else
					{
						// Handle case where file path could not be resolved
						std::cout << "❌ No file found for path '" << requestPath << "' in server: " << server.host << ":" << server.port << "\n";
						ResolutionResult errorPage = server.getErrorPage(404, *matchedLocation);
						if (!errorPage.path.empty())
							std::cout << "Serving custom 404 error page: " << errorPage.path << "\n";
						else
							std::cout << "❌ No custom 404 error page configured for server: " << server.host << ":" << server.port << "\n";
					}
				}
				else
					std::cout << "❌ No matching location found for path '" << requestPath << "' in server: " << server.host << ":" << server.port << "\n";
			}
			std::cout << std::endl;

			Server sv = Server(configManager);
			sv.initialize();
			sv.run();
		}
		catch (const std::exception &e)
		{
			std::cerr << "❌ Failed to parse " << argv[1] << "\n";
			std::cerr << e.what() << std::endl;
			return 1;
		}
	}
	else
	{
		std::cerr << "Usage: " << argv[0] << " <config_file_path>" << std::endl;
		return 1;
	}

	return 0;
}
