#ifndef STATUS_CODES_HPP
# define STATUS_CODES_HPP

# include <string>
# include <unordered_set>

namespace StatusCodes {
    enum Code {
        OK = 200,
        CREATED = 201,
        NO_CONTENT = 204,
        MOVED_PERMANENTLY = 301,
        BAD_REQUEST = 400,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        METHOD_NOT_ALLOWED = 405,
        CONFLICT = 409,
        PAYLOAD_TOO_LARGE = 413,
        INTERNAL_SERVER_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        GATEWAY_ERROR = 504
    };

    std::string getMessage(Code code);
};
    
#endif