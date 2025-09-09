#include <Buffer.hpp>
#include <string>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <iostream>

const size_t Buffer::DEFAULT_CAPACITY = 8192;
const size_t Buffer::MAX_CAPACITY = 1048576;

Buffer::Buffer()
{
	this->_readPos = 0;
	this->_writePos = 0;
	this->_capacity = DEFAULT_CAPACITY;
	this->_data.resize(DEFAULT_CAPACITY);
}

Buffer::~Buffer()
{
}

void Buffer::write(const void *data, size_t size)
{
	this->append((const char *)data, size);
}

size_t Buffer::read(void *data, size_t size)
{
	size_t availableBytes = this->_writePos - this->_readPos;
	size_t bytesToRead = std::min(availableBytes, size);

	if (bytesToRead == 0)
	{
		return 0;
	}

	const char *dt = this->_data.data() + this->_readPos;
	std::cout << "DT: " << dt << std::endl;
	memcpy(data, this->_data.data() + this->_readPos, bytesToRead);
	this->_readPos += bytesToRead;

	// Compact buffer if it's mostly read
	if (this->_readPos > this->_capacity / 2)
	{
		this->compact();
	}

	return bytesToRead;
}

size_t Buffer::peek(void *data, size_t size) const
{
	size_t availableBytes = this->_writePos - this->_readPos;
	size_t bytesToRead = std::min(availableBytes, size);

	if (bytesToRead == 0)
	{
		return 0;
	}

	memcpy(data, this->_data.data() + this->_readPos, bytesToRead);
	return bytesToRead;
}

void Buffer::append(const std::string &str)
{
	this->append(str.c_str(), str.size());
}

void Buffer::append(const char *data, size_t size)
{
	this->ensureSpace(size);
	std::copy(data, data + size, this->_data.begin() + this->_writePos);
	this->_writePos += size;
}

std::string Buffer::extractString(size_t length)
{
	if (this->available() < length)
	{
		throw std::runtime_error("Not enough data available to extract a string of size 'length'");
	}

	// TODO: Test
	std::string response = std::string(this->_data.begin() + this->_readPos, this->_data.begin() + this->_readPos + length);
	this->_readPos += length;

	return response;
}

std::string Buffer::extractAll()
{
	std::string response = std::string(this->_data.begin() + this->_readPos, this->_data.begin() + this->_writePos);
	this->_readPos = this->_writePos;
	return response;
}

size_t Buffer::available() const
{
	return this->_writePos - this->_readPos;
}

size_t Buffer::space() const
{
	return this->_capacity - this->_writePos;
}

size_t Buffer::capacity() const
{
	return this->_capacity;
}

bool Buffer::empty() const
{
	return this->available() == 0;
}

bool Buffer::full() const
{
	return this->_writePos == this->_capacity;
}

void Buffer::clear()
{
	this->_data.clear();
	this->_readPos = 0;
	this->_writePos = 0;
	this->resize(DEFAULT_CAPACITY);
	this->_capacity = DEFAULT_CAPACITY;
}

void Buffer::compact()
{
	// If there's no unread data or read position is already at start, no need to compact
	if (_readPos == 0 || _readPos >= _writePos)
	{
		return;
	}

	// Calculate amount of unread data
	size_t unreadBytes = _writePos - _readPos;

	// Move unread data to the beginning of the buffer
	// Using memmove instead of memcpy because source and destination might overlap
	std::memmove(_data.data(), _data.data() + _readPos, unreadBytes);

	// Update positions
	_readPos = 0;
	_writePos = unreadBytes;
}

bool Buffer::resize(size_t newCapacity)
{
	if (newCapacity > this->MAX_CAPACITY)
	{
		return false;
	}

	// Calculate current data size
	size_t currentDataSize = _writePos - _readPos;

	// If new capacity is smaller than current data, we need to compact first
	if (newCapacity < currentDataSize)
	{
		return false; // Cannot shrink below current data size
	}

	// If shrinking and have unread data not at beginning, compact first
	if (newCapacity < this->_capacity && this->_readPos > 0)
	{
		this->compact();
	}

	// Resize the vector
	this->_data.resize(newCapacity);
	this->_capacity = newCapacity;

	// Ensure positions are still valid
	if (this->_writePos > this->_capacity)
	{
		this->_writePos = this->_capacity;
	}
	if (this->_readPos > this->_writePos)
	{
		this->_readPos = this->_writePos;
	}

	return true;
}

bool Buffer::reserve(size_t minCapacity)
{
	// If we already have enough capacity, no need to resize
	if (this->_capacity >= minCapacity)
	{
		return true;
	}

	// Don't exceed maximum capacity
	if (minCapacity > MAX_CAPACITY)
	{
		minCapacity = MAX_CAPACITY;
	}

	// Choose growth strategy: double current capacity or use minCapacity, whichever one is larger
	size_t newCapacity = std::max(this->_capacity * 2, minCapacity);

	// But don't exceed MAX_CAPACITY
	newCapacity = std::min(newCapacity, MAX_CAPACITY);

	return this->resize(newCapacity);
}

std::string Buffer::extractLine()
{
	size_t pos = this->find("\r\n\r\n");

	if (pos != std::string::npos)
	{
		std::string line(this->_data.begin() + this->_readPos, this->_data.begin() + this->_readPos + pos);
		this->_readPos += pos + 1; // Skip the \n
		return line;
	}

	return ""; // No complete line
}

bool Buffer::hasCompleteHeaders() const
{
	return this->find("\r\n\r\n") != std::string::npos || this->find("\n\n") != std::string::npos;
}

void Buffer::ensureSpace(size_t needed)
{
	if (this->space() >= needed)
	{
		return;
	}

	// Try Compacting First
	if (this->_readPos > 0)
	{
		this->compact();
		if (this->space() >= needed)
		{
			return;
		}
	}

	// Need to resize
	size_t newCapacity = std::max(this->_capacity * 2, this->_capacity + needed);
	newCapacity = std::min(newCapacity, MAX_CAPACITY);

	if (newCapacity <= this->_capacity)
	{
		throw std::runtime_error("Buffer capacity limit exceeded");
	}

	this->resize(newCapacity);
}

size_t Buffer::find(const std::string &pattern) const
{
	// Handle Edge Cases
	if (pattern.empty() || this->available() == 0)
	{
		return std::string::npos;
	}

	// If pattern is longer than available data, it's impossible to find it
	if (pattern.size() > this->available())
	{
		return std::string::npos;
	}

	// Get pointers to the search range (unread data only)
	const char *start = this->_data.data() + this->_readPos;
	const char *end = this->_data.data() + this->_writePos;

	// Use std::search to find the pattern
	const char *result = std::search(
		start, end,
		pattern.begin(), pattern.end());

	// If found, return position relative to read position
	if (result != end)
	{
		return static_cast<size_t>(result - start);
	}

	return std::string::npos;
}

size_t Buffer::find(char c) const
{
	if (this->available() == 0)
	{
		return std::string::npos;
	}

	// Get pointers to the search range (unread data only)
	const char *start = this->_data.data() + this->_readPos;
	const char *end = this->_data.data() + this->_writePos;

	// Use std::find which respects the range
	const char *result = std::find(start, end, c);

	// If found, return position relative to read position
	if (result != end)
	{
		return static_cast<size_t>(result - start);
	}

	return std::string::npos;
}

bool Buffer::contains(const std::string &pattern) const
{
	return this->find(pattern) != std::string::npos;
}

size_t Buffer::getHeadersSize() const
{
	size_t headersTerminatorPos = this->find(HEADERS_TERMINATOR);

	if (headersTerminatorPos == std::string::npos)
	{
		return 0;
	}

	return headersTerminatorPos + std::strlen(HEADERS_TERMINATOR);
}

std::string Buffer::getHeaders() const
{
	size_t headersSize = this->getHeadersSize();
	if (headersSize == 0)
	{
		return "";
	}

	return std::string(reinterpret_cast<const char *>(this->_data.data()) + this->_readPos, headersSize);
}

// char *Buffer::writePtr() {

// }

const char *Buffer::data() const
{
	return this->_data.data();
}
