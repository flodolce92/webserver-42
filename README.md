# 🚀 Webserv

Webserv is a minimalist HTTP/1.1 server implementation written in C++. Developed as part of the 42 School curriculum, the project focuses on understanding fundamental networking principles and the HTTP protocol. Our server is capable of handling standard requests, serving static files, managing custom error pages, and, eventually, supporting CGI.

-----

### ⚙️ How to Compile and Run

To compile the project, ensure you have `make` and a C++98-compatible compiler (such as `g++`) installed.

1.  Clone the repository:
    ```bash
    git clone https://github.com/tuo-username/tuo-repo.git
    cd tuo-repo
    ```
2.  Run the `make` command:
    ```bash
    make
    ```
    This will create the `webserv` executable.
3.  Run the server with a configuration file:
    ```bash
    ./webserv path/to/your/config.conf
    ```
    An example configuration file (`advanced_config.conf`) is included in the repository to help you test all the functionalities.

-----

### 📋 Features

  * **HTTP/1.1 Server**: Compliant with the basic standards of the protocol.
  * **Multi-Client Handling**: Manages multiple client connections concurrently using multiplexing.
  * **Virtual Servers**: Supports handling multiple servers on the same port via the `Host` header.
  * **`location`-Based Routing**: Flexible configuration for managing URI paths.
  * **Static Files**: Serves HTML, CSS, JavaScript, and image files.
  * **Custom Error Pages**: Handles errors like `404 Not Found` and `403 Forbidden` with dedicated pages.
  * **Directory Listing (`autoindex`)**: Automatically generates a list of files in a directory.

-----

### 🤝 Core Division Strategy

The project was developed with a clear separation of tasks to ensure code modularity and development efficiency. This strategy reflects the original plan for a three-person team.

#### **Member 1: Network Core & I/O Multiplexing**

**Username:** [Kuba2901](https://github.com/Kuba2901)
**Responsibilities:**

  - Socket programming (server socket creation, binding, listening)
  - I/O multiplexing implementation (`select`/`poll`/`epoll`)
  - Client connection management and non-blocking I/O
  - Request reception and response transmission
  - Connection lifecycle management
  - HTTP-specific features (keep-alive)

**Key Deliverables:**

  - Server class with socket handling
  - Connection manager for multiple clients
  - Network buffer management and chunking

-----

#### **Member 2: HTTP Parser & Response Builder**

**Username:** [micongiu](https://github.com/micongiu)
**Responsibilities:**

  - HTTP response generation (status codes, headers, body)
  - Protocol validation and error handling
  - Request routing to appropriate handlers
  - HTTP-specific features (chunked encoding, keep-alive)

**Key Deliverables:**

  - Response builder class
  - HTTP protocol compliance and validation

-----

#### **Member 3: Configuration & CGI**

**Username:** [flodolce92](https://github.com/flodolce92)
**Responsibilities:**

  - Configuration file parsing (Nginx-like syntax)
  - HTTP request parsing (headers, body, methods)
  - Server configuration management
  - CGI implementation and external process handling
  - File serving logic for static content
  - Error page handling and logging

**Key Deliverables:**

  - Configuration parser
  - CGI handler class
  - Static file server
  - Error handling system
  - Request parser class

-----

### 🤝 Contributors

This project was developed by:

  * [Kuba2901](https://github.com/Kuba2901)
  * [micongiu](https://github.com/micongiu)
  * [flodolce92](https://github.com/flodolce92)
