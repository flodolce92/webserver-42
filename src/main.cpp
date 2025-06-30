#include <Buffer.hpp>
#include <iostream>
#include <string>
#include <cassert>
#include <cstring>

void testBasicOperations()
{
	std::cout << "Testing basic operations..." << std::endl;

	Buffer buffer;

	// Test initial state
	assert(buffer.available() == 0);
	assert(buffer.empty() == true);
	assert(buffer.full() == false);
	assert(buffer.capacity() > 0);

	// Test append and available
	std::string testData = "Hello World";
	buffer.append(testData);
	assert(buffer.available() == testData.size());
	assert(buffer.empty() == false);

	// Test read
	char readBuffer[100];
	size_t bytesRead = buffer.read(readBuffer, sizeof(readBuffer));
	readBuffer[bytesRead] = '\0';
	assert(bytesRead == testData.size());
	assert(std::string(readBuffer) == testData);
	assert(buffer.available() == 0);

	std::cout << "✓ Basic operations test passed" << std::endl;
}

void testStringOperations()
{
	std::cout << "Testing string operations..." << std::endl;

	Buffer buffer;

	// Test append with string and char array
	buffer.append("Hello");
	buffer.append(" ", 1);
	buffer.append(std::string("World"));

	assert(buffer.available() == 11);

	// Test extractString
	std::string extracted = buffer.extractString(5);
	assert(extracted == "Hello");
	assert(buffer.available() == 6);

	// Test extractAll
	std::string remaining = buffer.extractAll();
	assert(remaining == " World");
	assert(buffer.available() == 0);

	std::cout << "✓ String operations test passed" << std::endl;
}

void testBufferManagement()
{
	std::cout << "Testing buffer management..." << std::endl;

	Buffer buffer;

	// Test clear
	buffer.append("Test data");
	assert(buffer.available() > 0);
	buffer.clear();
	assert(buffer.available() == 0);
	assert(buffer.empty() == true);

	// Test resize and reserve
	size_t initialCapacity = buffer.capacity();
	assert(buffer.reserve(initialCapacity * 2) == true);
	assert(buffer.capacity() >= initialCapacity * 2);

	std::cout << "✓ Buffer management test passed" << std::endl;
}

void testSearchOperations()
{
	std::cout << "Testing search operations..." << std::endl;

	Buffer buffer;
	buffer.append("Hello World, this is a test");

	// Test find string
	assert(buffer.find("World") == 6);
	assert(buffer.find("test") == 23);
	assert(buffer.find("notfound") == std::string::npos);

	// Test find char
	assert(buffer.find('W') == 6);
	assert(buffer.find('x') == std::string::npos);

	// Test contains
	assert(buffer.contains("Hello") == true);
	assert(buffer.contains("notfound") == false);

	std::cout << "✓ Search operations test passed" << std::endl;
}

void testHTTPHelpers()
{
	std::cout << "Testing HTTP helpers..." << std::endl;

	Buffer buffer;

	// Test without complete headers
	buffer.append("GET / HTTP/1.1\r\nHost: example.com\r\n");
	assert(buffer.hasCompleteHeaders() == false);
	assert(buffer.getHeadersSize() == 0);

	// Test with complete headers
	buffer.append("\r\n");
	assert(buffer.hasCompleteHeaders() == true);
	assert(buffer.getHeadersSize() > 0);

	std::string headers = buffer.getHeaders();
	assert(headers.find("GET / HTTP/1.1") != std::string::npos);
	assert(headers.find("Host: example.com") != std::string::npos);

	std::cout << "✓ HTTP helpers test passed" << std::endl;
}

void testPeekOperation()
{
	std::cout << "Testing peek operation..." << std::endl;

	Buffer buffer;
	buffer.append("Test data for peek");

	char peekBuffer[10];
	size_t peeked = buffer.peek(peekBuffer, sizeof(peekBuffer));
	peekBuffer[peeked] = '\0';

	// Peek should not change available data
	assert(buffer.available() == 18);
	assert(std::string(peekBuffer) == "Test data ");

	// Read should still work normally
	char readBuffer[10];
	size_t read = buffer.read(readBuffer, sizeof(readBuffer));
	readBuffer[read] = '\0';
	assert(std::string(readBuffer) == "Test data ");
	assert(buffer.available() == 8);

	std::cout << "✓ Peek operation test passed" << std::endl;
}

void testBufferCompaction()
{
	std::cout << "Testing buffer compaction..." << std::endl;

	Buffer buffer;
	buffer.append("0123456789abcdef");

	// Read some data to create gap at beginning
	char temp[8];
	buffer.read(temp, 8);
	assert(buffer.available() == 8);

	// Add more data to trigger compaction during ensureSpace
	buffer.append("ghijklmnopqrstuv");
	assert(buffer.available() == 24);

	// Extract all and verify data integrity
	std::string result = buffer.extractAll();
	assert(result == "89abcdefghijklmnopqrstuv");

	std::cout << "✓ Buffer compaction test passed" << std::endl;
}

void testErrorConditions()
{
	std::cout << "Testing error conditions..." << std::endl;

	Buffer buffer;

	// Test extractString with insufficient data
	try
	{
		buffer.extractString(100);
		assert(false); // Should not reach here
	}
	catch (const std::runtime_error &e)
	{
		// Expected
	}

	// Test read with empty buffer
	char temp[10];
	size_t read = buffer.read(temp, sizeof(temp));
	assert(read == 0);

	// Test peek with empty buffer
	size_t peeked = buffer.peek(temp, sizeof(temp));
	assert(peeked == 0);

	std::cout << "✓ Error conditions test passed" << std::endl;
}

int main()
{
	std::cout << "Starting Buffer tests..." << std::endl
			  << std::endl;

	testBasicOperations();
	testStringOperations();
	testBufferManagement();
	testSearchOperations();
	testHTTPHelpers();
	testPeekOperation();
	testBufferCompaction();
	testErrorConditions();

	std::cout << std::endl
			  << "🎉 All Buffer tests passed!" << std::endl;

	return 0;
}