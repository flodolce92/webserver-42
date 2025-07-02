#include <ConfigParser.hpp>
#include <Server.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
	if (argc == 2)
	{
		try
		{
			ConfigParser parser;
			Config config = parser.parseFile(argv[1]);

			std::cout << "✅ Successfully parsed: " << argv[1] << "\n";
			parser.printConfig(config);
			std::cout << std::endl;
		}
		catch (const std::exception &e)
		{
			std::cout << "❌ Failed to parse " << argv[1] << ":\n";
			std::cout << "Error: " << e.what() << "\n";
			return 1;
		}
	}

	Server sv = Server(8080);
	sv.initialize();
	sv.run();

	return 0;
}
