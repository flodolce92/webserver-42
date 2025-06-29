#include <Buffer.hpp>
#include <string>

Buffer::Buffer(size_t initialCapacity = DEFAULT_CAPACITY) {

}

Buffer::~Buffer() {

}


size_t Buffer::write(const void *data, size_t size) {
	this->append((const char *)data, size);
}

size_t Buffer::read(void *data, size_t size) {
	size_t availableBytes = this->_writePos - this->_readPos;
	size_t bytesToRead = std::min(availableBytes, size);
	
	if (bytesToRead == 0) {
		return 0;
	}
	
	std::memcpy(data, &this->_data[this->_readPos], bytesToRead);
	this->_readPos += bytesToRead;
	
	// Compact buffer if it's mostly read
	if (this->_readPos > this->_capacity / 2) {
		this->compact();
	}
	
	return bytesToRead;
}

size_t Buffer::peek(void *data, size_t size) const {
	size_t availableBytes = this->_writePos - this->_readPos;
	size_t bytesToRead = std::min(availableBytes, size);
	
	if (bytesToRead == 0) {
		return 0;
	}
	
	std::memcpy(data, &this->_data[this->_readPos], bytesToRead);	
	return bytesToRead;
}


void Buffer::append(const std::string &str) {
	this->append(str.c_str(), str.size());
}

void Buffer::append(const char *data, size_t size) {
	this->ensureSpace(size);
	std::copy(data, data + size, this->_data.begin() + this->_writePos);
	this->_writePos += size;
}

std::string Buffer::extractLine() {
	size_t pos = this->find("\r\n\r\n");

	if (pos != std::string::npos) {
		std::string line(this->_data.begin() + this->_readPos, this->_data.begin() + this->_readPos + pos);
		this->_readPos += pos + 1; // Skip the \n
		return line;
	}

	return ""; // No complete line
}

bool Buffer::hasCompleteHeaders() const {
	return this->find("\r\n\r\n") != std::string::npos || this->find("\n\n") != std::string::npos;
}

void Buffer::ensureSpace(size_t needed) {
	if (this->space() >= needed) {
		return ;
	}

	// Try Compacting First
	if (this->_readPos > 0) {
		this->compact();
		if (this->space() >= needed) {
			return ;
		}
	}

	// Need to resize
	size_t newCapacity = std::max(this->_capacity * 2, this->_capacity + needed);
	newCapacity = std::min(newCapacity, MAX_CAPACITY);

	if (newCapacity <= this->_capacity) {
		throw std::runtime_error("Buffer capacity limit exceeded");
	}

	this->resize(newCapacity);
}