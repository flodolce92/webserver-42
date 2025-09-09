#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>

#define READ_END 0
#define WRITE_END 1

class CGIHandler
{
private:
	// Private constructor to prevent instantiation
	CGIHandler();
	~CGIHandler();

	// Helper functions to convert std::map to char** for execve
	static char **mapToCharPtrArray(const std::map<std::string, std::string> &map);
	static void freeCharPtrArray(char **arr);

	// Executes the child process
	static void childProcess(int *pipe_in, int *pipe_out, const std::string &scriptPath,
							 char **envp, char **argv);

	// Reads the output from the pipe
	static std::string readFromPipe(int pipe_fd);

	// Writes request data to the pipe
	static void writeToPipe(int pipe_fd, const std::string &data);

public:
	// Main method to execute the CGI script
	// Takes the script path, request data (for POST),
	// and the required environment variables.
	static std::string executeCGI(const std::string &scriptPath,
								  const std::string &requestBody,
								  const std::map<std::string, std::string> &envVars);
};

#endif
