#include "StatusCodes.hpp"

namespace StatusCodes
{
	std::string getMessage(Code code)
	{
		switch (code)
		{
		case OK:
			return "OK";
		case CREATED:
			return "Created";
		case NO_CONTENT:
			return "No Content";
		case MOVED_PERMANENTLY:
			return "Moved Permanently";
		case BAD_REQUEST:
			return "Bad Request";
		case FORBIDDEN:
			return "Forbidden";
		case NOT_FOUND:
			return "Not Found";
		case METHOD_NOT_ALLOWED:
			return "Method Not Allowed";
		case CONFLICT:
			return "Conflict";
		case PAYLOAD_TOO_LARGE:
			return "Payload Too Large";
		case INTERNAL_SERVER_ERROR:
			return "Internal Server Error";
		case NOT_IMPLEMENTED:
			return "Not Implemented";
		case GATEWAY_ERROR:
			return "Gateway Timeout";
		default:
			return "Unknown Status Code";
		}
	}
}