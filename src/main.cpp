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

			std::cout << "✅ Successfully parsed: " << argv[1] << "\\n";
			configManager.printConfiguration();
			std::cout << std::endl;

			Server sv = Server(configManager);
			sv.initialize();
			sv.run();
		}
		catch (const std::exception &e)
		{
			std::cout << "❌ Failed to parse " << argv[1] << ":\\n";
			std::cout << "Error: " << e.what() << "\\n";
			return 1;
		}
	} else {
        std::cerr << "Usage: " << argv[0] << " <config_file_path>" << std::endl;
        return 1;
    }

	return 0;
}