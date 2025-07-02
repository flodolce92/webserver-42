#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <vector>
#include <string>

#define HEADERS_TERMINATOR "\r\n\r\n"

class Buffer
{
private:
	std::vector<char> _data;
	size_t _readPos;
	size_t _writePos;
	size_t _capacity;

	static const size_t DEFAULT_CAPACITY;
	static const size_t MAX_CAPACITY;

	void ensureSpace(size_t needed);

public:
	Buffer();
	~Buffer();

	// Basic Operations
	void write(const void *data, size_t size);
	size_t read(void *data, size_t size);
	size_t peek(void *data, size_t size) const;

	// String Operations
	void append(const std::string &str);
	void append(const char *data, size_t size);
	std::string extractString(size_t length);
	std::string extractLine(); // Extract Until \r\n or \n
	std::string extractAll();

	// Buffer State
	size_t available() const;
	size_t space() const;
	size_t capacity() const;
	bool empty() const;
	bool full() const;

	// Buffer Management
	void clear();
	void compact(); // Move unread data to beginning
	bool resize(size_t newCapacity);
	bool reserve(size_t minCapacity);

	// Search Operations
	size_t find(const std::string &pattern) const;
	size_t find(char c) const;
	bool contains(const std::string &pattern) const;

	// HTTP-Specific Helpers
	bool hasCompleteHeaders() const;
	size_t getHeadersSize() const;
	std::string getHeaders() const;

	// Direct Access (use carefully)
	const char *data() const;
	// char *writePtr();
};

#endif