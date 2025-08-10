#ifndef STATUS_CODES_HPP
# define STATUS_CODES_HPP

# include <string>

namespace StatusCodes {
    struct StatusCode {
        int         code;
        std::string message;
    };

    static StatusCode OK = {200, "OK"};
    static StatusCode CREATED = {201, "CREATED"};
    static StatusCode NO_CONTENT = {204, "NO_CONTENT"};
    static StatusCode MOVED_PERMANENTLY = {301, "MOVED PERMANENTLY"};
    static StatusCode BAD_REQUEST = {400, "BAD REQUEST"};
    static StatusCode FORBIDDEN = {403, "FORBIDDEN"};
    static StatusCode NOT_FOUND = {404, "NOT FOUND"};
    static StatusCode METHOD_NOT_ALLOWED = {405, "METHOD NOT ALLOWED"};
    static StatusCode CONFLICT = {409, "CONFLICT"};
    static StatusCode PAYLOAD_TOO_LARGE = {413, "PAYLOAD TOO LARGE"};
    static StatusCode INTERNAL_SERVER_ERROR = {500, "INTERNAL SERVER ERROR"};
    static StatusCode NOT_IMPLEMENTED = {501, "NOT IMPLEMENTED"};
    static StatusCode GATEWAY_ERROR = {504, "GATEWAY ERROR"};
};

#endif