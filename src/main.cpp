#include <ConfigManager.hpp>
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

			// Test route finder on first server
			const std::vector<ServerConfig> &servers = configManager.getServers();
			std::string requestPath = "/api/index.html";
			if (!servers.empty())
			{
				const ServerConfig &server = servers[0];
				const Route *matchedRoute = server.findMatchingRoute(requestPath);
				if (matchedRoute)
				{
					std::cout << "✅ Matched route for path '" << requestPath << "':\n";
					std::cout << "Path: " << matchedRoute->path << "\n";
					std::cout << "Root: " << matchedRoute->root << "\n";
				}
				else
					std::cout << "❌ No matching route found for path '" << requestPath << "' in server: " << server.host << ":" << server.port << "\n";
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
