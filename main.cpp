#include <iostream>
#include <unistd.h>
#include <string>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <csignal>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <fstream>
#include <iostream>

#include <sstream>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>


#define DEFAULT_PORT 8080
// Buffer size for reading client requests
#define BUFFER_SIZE 1024
// HTML file to serve
#define HTML_FILE "index.html"

// Helper function to log errors and exit
void error_exit(const char* msg)
{
	std::cerr << (msg) << "\n";
	exit(EXIT_FAILURE);
}

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

	std::cout << "Server listening on port " << DEFAULT_PORT << "..." << "\n";
	std::cout << "Serving file: " << HTML_FILE << "\n";

	// 6. Main accept loop
	while (true)
	{
		std::cout << "\nWaiting for a new connection..." << "\n";

		if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0)
		{
			std::cerr << "accept failed" << "\n";
			continue;
		}

		long bytes_read = read(new_socket, buffer, BUFFER_SIZE - 1);

		if (bytes_read < 0)
		{
			std::cerr << "read failed" << "\n";
			close(new_socket);
			continue;
		}
		else if (bytes_read == 0)
		{
			std::cout << "Client disconnected before sending data." << "\n";
			close(new_socket);
			continue;
		}
		buffer[bytes_read] = '\0';

		// 8. Construct HTTP response
		std::string request_str(buffer);
		std::string http_response;
		std::string html_content;

		// Try to read the HTML file
		std::ifstream html_file(HTML_FILE);
		if (html_file.is_open())
		{
			
			if (request_str.substr(0, 3) == "GET")
			{
				std::string file_content_str;
				struct stat stat_buf;
				int rc = stat(HTML_FILE, &stat_buf);
				if (rc != 0)
				{
					std::cerr << "Error (stat) getting size for '" << HTML_FILE << "': " << strerror(errno) << std::endl;
					return 1;
				}
				long long file_size = stat_buf.st_size;

				if (access(HTML_FILE, F_OK | R_OK) == -1) {
				std::cerr << "Error: File '" << HTML_FILE << "' either does not exist or is not readable. (" << strerror(errno) << ")" << std::endl;
				return 1;
				}

				int fd = open(HTML_FILE, O_RDONLY);
				if (fd == -1)
				{
					std::cerr << "Error opening file '" << HTML_FILE << "'. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
					return 1;
				}
				
				file_content_str.resize(static_cast<std::string::size_type>(file_size));
				 char* string_buffer = &file_content_str[0];

				ssize_t total_bytes_read = 0;
				ssize_t bytes_read_this_call;

				while (total_bytes_read < file_size)
				{
					size_t bytes_to_read = static_cast<size_t>(file_size - total_bytes_read);
					if (bytes_to_read == 0)
						break;

					bytes_read_this_call = read(fd, string_buffer + total_bytes_read, bytes_to_read);

					if (bytes_read_this_call < 0)
					{
						std::cerr << "Error (read) from '" << HTML_FILE << "': " << strerror(errno) << std::endl;
						file_content_str.clear();
						break;
					}
					total_bytes_read += bytes_read_this_call;
				}

				if (close(fd) == -1)
				{
					std::cerr << "Error closing file '" << HTML_FILE << "'. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
					return 1;
				}
				
				std::string status_line = "HTTP/1.1 200 OK\r\n";
				http_response = status_line + "\r\n" + file_content_str;
			}
			else
				std::cerr << "Only the Get is working" << "\n";
		}
		else
			std::cerr << "error 500" << "\n";

		// 9. Send the HTTP response
		long bytes_sent = write(new_socket, http_response.c_str(), http_response.length());
		if (bytes_sent < 0)
			std::cerr << "write failed" << "\n";
		else if (static_cast<size_t>(bytes_sent) < http_response.length())
			std::cerr << "Warning: Not all data was sent to the client." << "\n";
		else
			std::cout << "Response sent successfully." << "\n";

		// 10. Close the client socket
		close(new_socket);
		std::cout << "Client socket closed." << "\n";
	}

	return 0;
}