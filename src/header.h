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

void readFile(const char* fileName, int newSocket);
void sendData(int newSocket, char* dataToSend, size_t len);