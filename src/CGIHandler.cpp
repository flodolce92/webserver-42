#include <CGIHandler.hpp>
#include <iostream>
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <errno.h>

CGIHandler::CGIHandler() {}

CGIHandler::~CGIHandler() {}

char **CGIHandler::mapToCharPtrArray(const std::map<std::string, std::string> &map)
{
	char **envp = new char *[map.size() + 1];
	int i = 0;
	for (std::map<std::string, std::string>::const_iterator it = map.begin(); it != map.end(); ++it)
	{
		std::string env_str = it->first + "=" + it->second;
		envp[i] = strdup(env_str.c_str()); // TODO: Check if strdup is permitted
		i++;
	}
	envp[i] = NULL;
	return envp;
}

void CGIHandler::freeCharPtrArray(char **arr)
{
	for (int i = 0; arr[i] != NULL; ++i)
	{
		free(arr[i]);
	}
	delete[] arr;
}

void CGIHandler::childProcess(int *pipe_in, int *pipe_out, const std::string &scriptPath,
							  char **envp, char **argv)
{
	close(pipe_in[WRITE_END]);
	close(pipe_out[READ_END]);

	// Redirect stdin (0) to the pipe_in (for input to CGI script)
	if (dup2(pipe_in[READ_END], STDIN_FILENO) == -1)
	{
		std::cerr << "dup2 stdin failed: " << strerror(errno) << std::endl;
		exit(EXIT_FAILURE);
	}
	close(pipe_in[READ_END]); // Close the original now that stdin is redirected

	// Redirect stdout (1) to the pipe_out (for output from CGI script)
	if (dup2(pipe_out[WRITE_END], STDOUT_FILENO) == -1)
	{
		std::cerr << "dup2 stdout failed: " << strerror(errno) << std::endl;
		exit(EXIT_FAILURE);
	}
	close(pipe_out[WRITE_END]); // Close the original now that stdout is redirected

	// Execute the CGI script using execve
	// argv[0] is the script path, argv[1] onwards can be used
	// if the script expects command-line arguments.
	// envp is the environment variables passed to the script.
	if (execve(scriptPath.c_str(), argv, envp) == -1)
	{
		std::cerr << "execve failed: " << strerror(errno) << std::endl;
		exit(EXIT_FAILURE);
	}
}

std::string CGIHandler::readFromPipe(int pipe_fd)
{
	std::string output;
	char buffer[4096];
	ssize_t bytesRead;

	while ((bytesRead = read(pipe_fd, buffer, sizeof(buffer) - 1)) > 0)
	{
		buffer[bytesRead] = '\0';
		output.append(buffer, bytesRead);
	}
	if (bytesRead == -1)
	{
		std::cerr << "read from pipe failed: " << strerror(errno) << std::endl;
		throw std::runtime_error("Error reading from CGI pipe.");
	}
	return output;
}

void CGIHandler::writeToPipe(int pipe_fd, const std::string &data)
{
	if (!data.empty())
	{
		ssize_t bytesWritten = write(pipe_fd, data.c_str(), data.length());
		if (bytesWritten == -1)
		{
			std::cerr << "write to pipe failed: " << strerror(errno) << std::endl;
			throw std::runtime_error("Error writing to CGI pipe.");
		}
	}
	close(pipe_fd); // Close the pipe to signal EOF to the child process
}

std::string CGIHandler::executeCGI(const std::string &scriptPath,
								   const std::string &requestBody,
								   const std::map<std::string, std::string> &envVars)
{
	int pipe_in[2];	 // Server -> CGI
	int pipe_out[2]; // CGI -> Server

	if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1)
	{
		std::cerr << "pipe creation failed: " << strerror(errno) << std::endl;
		throw std::runtime_error("Failed to create pipes for CGI.");
	}

	// Prepare argv: the first element is the script path
	char *argv[] = {strdup(scriptPath.c_str()), NULL};

	// Prepare the environment variables for execve
	char **envp = mapToCharPtrArray(envVars);

	pid_t pid = fork();

	if (pid == -1)
	{
		std::cerr << "fork failed: " << strerror(errno) << std::endl;
		freeCharPtrArray(envp);
		free(argv[0]);
		close(pipe_in[0]);
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_out[1]);
		throw std::runtime_error("Failed to fork for CGI.");
	}
	else if (pid == 0)
	{
		childProcess(pipe_in, pipe_out, scriptPath, envp, argv);
		exit(EXIT_FAILURE);
	}
	else
	{
		// Close the file descriptors that are not used in the parent process
		close(pipe_in[READ_END]);
		close(pipe_out[WRITE_END]);

		// Write the request body to the stdin of the child process
		writeToPipe(pipe_in[1], requestBody);

		// Read the output from the stdout of the child process
		std::string cgiOutput = readFromPipe(pipe_out[0]);
		close(pipe_out[0]);

		// Wait for the child process to finish
		int status;
		waitpid(pid, &status, 0);

		// Free the environment variables and script path
		freeCharPtrArray(envp);
		free(argv[0]);

		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		{
			std::cout << "CGI script executed successfully." << std::endl;
			return cgiOutput;
		}
		else
		{
			std::cerr << "CGI script failed or terminated abnormally. Status: " << status << std::endl;
			throw std::runtime_error("CGI execution failed.");
		}
	}
}
