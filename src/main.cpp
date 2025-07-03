#include "header.h"
#include "Response.hpp"

#define DEFAULT_PORT 8080
// Buffer size for reading client requests
#define BUFFER_SIZE 1024
// HTML file to serve
#define HTML_FILE "index.html"

// Helper function to log errors and exit
void error_exit(const char* msg)
{
	std::cerr << (msg) << std::endl;
	exit(EXIT_FAILURE);
}

// Helper function to close the server smoothly
void handle_sigint(int signum)
{
	const char msg[] = "\nShutting down server..\n";
	write(STDOUT_FILENO, msg, sizeof(msg) - 1);
	std::exit(signum);
}

int main()
{
	int server_fd;
	int new_socket;
	struct sockaddr_in address;
	int opt = 1;
	socklen_t addrlen = sizeof(address);
	char buffer[BUFFER_SIZE] = {0};

	// Creating the sigaction to shut down the server in an better way
	signal(SIGINT, handle_sigint);

	// 1. Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
		error_exit("socket failed");

	// 2. Set socket options
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
		error_exit("setsockopt SO_REUSEADDR failed");

	// 3. Define the server address structure
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(DEFAULT_PORT);

	// 4. Bind the socket
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
		error_exit("bind failed");

	// 5. Listen for incoming connections
	if (listen(server_fd, 10) < 0)
		error_exit("listen failed");

	std::cout << "Server listening on port " << DEFAULT_PORT << "..." << std::endl;
	std::cout << "Serving file: " << HTML_FILE << std::endl;

	// 6. Main accept loop
	while (true)
	{
		std::cout << "\nWaiting for a new connection..." << std::endl;

		if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0)
		{
			std::cerr << "accept failed" << std::endl;
			continue;
		}

		ssize_t bytes_read = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);

		if (bytes_read < 0)
		{
			std::cerr << "read failed" << std::endl;
			close(new_socket);
			continue;
		}
		else if (bytes_read == 0)
		{
			std::cerr << "Client disconnected before sending data." << std::endl;
			close(new_socket);
			continue;
		}
		buffer[bytes_read] = '\0';

		std::string request_str(buffer);
		std::string cleanRequest = request_str.substr(0, request_str.find("\n"));
		std::cout << cleanRequest << std::endl;

		size_t pos = 0;
		std::string sub;
		std::vector<std::string> vectorRequest;

		while (pos != std::string::npos)
		{
			pos = cleanRequest.find('/');
			sub = cleanRequest.substr(0, pos);
			vectorRequest.push_back(sub);
			cleanRequest.erase(0, pos + 1);
		}

		std::vector<std::string>::const_iterator const_it = vectorRequest.begin();
		if (*const_it == "GET " and request_str[4] != ' ')
		{
			vectorRequest.erase(vectorRequest.begin());
			vectorRequest.pop_back();
			(vectorRequest.back()).erase(((vectorRequest.back()).length() - 5), 5);

			std::string filename;
			std::vector<std::string>::const_iterator i = vectorRequest.begin();
			filename = *i;
			i++;
			for ( ; i != vectorRequest.end(); i++)
				filename += '/' + *i;

			Response response = Response(new_socket, 200, filename.c_str());
			response.readFile();
		}
		else if (*const_it == "POST " and request_str[5] != ' ')
		{
			std::cout << "POST" << std::endl;
		}
		else if (*const_it == "DELETE " and request_str[7] != ' ')
		{
			std::cout << "DELETE" << std::endl;
		}
		else
		{
			std::cout << "not support" << std::endl;
		}

		close(new_socket);
		std::cout << "Client socket closed." << std::endl;

	}
	return 0;
}
