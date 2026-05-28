# Design Document: Webserv HTTP Server

## Overview

The Webserv HTTP Server is a C++98-compliant HTTP/1.1 server designed for concurrent connection handling using non-blocking I/O multiplexing. The architecture is structured to support a 3-person development team with clearly defined component boundaries and shared data structures.

### System Goals

- Handle multiple concurrent HTTP connections efficiently using non-blocking I/O
- Parse and apply NGINX-style configuration files
- Serve static content, execute CGI scripts, and handle file uploads
- Support virtual servers on multiple ports
- Maintain strict C++98 compliance for academic requirements
- Provide clear component interfaces for parallel development

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     HTTP Server Core                         │
│  ┌────────────────┐  ┌──────────────┐  ┌─────────────────┐ │
│  │ IO_Multiplexer │  │ Connection   │  │ Virtual_Server  │ │
│  │ (poll/select/  │──│ Manager      │──│ Manager         │ │
│  │  epoll/kqueue) │  │              │  │                 │ │
│  └────────────────┘  └──────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ├─────────────────────────────┐
                              │                             │
                              ▼                             ▼
┌──────────────────────────────────────┐   ┌──────────────────────────────┐
│      Config Parser Subsystem         │   │  Request Handler Subsystem   │
│  ┌────────────────────────────────┐  │   │  ┌────────────────────────┐  │
│  │  Config_Parser                 │  │   │  │  Request_Handler       │  │
│  │  ┌──────────────────────────┐  │  │   │  │  ┌──────────────────┐  │  │
│  │  │ Server_Block_Parser      │  │  │   │  │  │ HTTP_Parser      │  │  │
│  │  └──────────────────────────┘  │  │   │  │  └──────────────────┘  │  │
│  │  ┌──────────────────────────┐  │  │   │  │  ┌──────────────────┐  │  │
│  │  │ Location_Block_Parser    │  │  │   │  │  │ Response_Builder │  │  │
│  │  └──────────────────────────┘  │  │   │  │  └──────────────────┘  │  │
│  └────────────────────────────────┘  │   │  │  ┌──────────────────┐  │  │
│  ┌────────────────────────────────┐  │   │  │  │ CGI_Executor     │  │  │
│  │  Config_Storage                │  │   │  │  └──────────────────┘  │  │
│  │  ┌──────────────────────────┐  │  │   │  └────────────────────────┘  │
│  │  │ ServerConfig (vector)    │  │  │   └──────────────────────────────┘
│  │  └──────────────────────────┘  │  │
│  │  ┌──────────────────────────┐  │  │
│  │  │ Location_Map (std::map)  │  │  │
│  │  └──────────────────────────┘  │  │
│  └────────────────────────────────┘  │
└──────────────────────────────────────┘
```


### Team Division Strategy

**Developer 1: Config Parser - Server Block Parsing**
- Implements Server_Block_Parser
- Parses server-level directives (listen, server_name, host, root, index, error_page, client_max_body_size)
- Populates ServerConfig structures
- Validates server block syntax

**Developer 2: Config Parser - Location Block Parsing**
- Implements Location_Block_Parser
- Parses location-level directives (allowed_methods, autoindex, return, cgi_path, cgi_extension, upload_dir)
- Populates Location_Map with LocationConfig structures
- Validates location block syntax
- Depends on Developer 1's ServerConfig structures

**Developer 3: Request/Response Handler**
- Implements Request_Handler, HTTP_Parser, Response_Builder, CGI_Executor
- Processes HTTP requests using parsed configuration
- Generates HTTP responses
- Handles static files, CGI execution, uploads, and directory listings
- Depends on Config_Storage interface from Developers 1 & 2

### Critical Shared Data Structures

All developers must agree on these structures before implementation begins:

```cpp
// ServerConfig: Stores server block configuration
struct ServerConfig {
    std::vector<int> listen_ports;
    std::string host;
    std::vector<std::string> server_names;
    std::string root;
    std::vector<std::string> index_files;
    std::map<int, std::string> error_pages;  // status_code -> file_path
    size_t client_max_body_size;
    std::map<std::string, LocationConfig> locations;  // Location_Map
};

// LocationConfig: Stores location block configuration
struct LocationConfig {
    std::string path;
    std::vector<std::string> allowed_methods;
    bool autoindex;
    std::string return_code;
    std::string return_url;
    std::string cgi_path;
    std::vector<std::string> cgi_extensions;
    std::string upload_dir;
    std::string root;  // Can override server root
    std::vector<std::string> index_files;  // Can override server index
};

// Config_Storage: Central configuration repository
class Config_Storage {
public:
    std::vector<ServerConfig> servers;
    
    const ServerConfig* getServerConfig(const std::string& host, int port) const;
    const LocationConfig* getLocationConfig(const ServerConfig* server, 
                                           const std::string& path) const;
};
```

## Architecture

### Component Breakdown

#### 1. Config Parser Subsystem

**Responsibility**: Parse configuration files and populate Config_Storage

**Sub-components**:

- **Config_Parser**: Orchestrates parsing, manages file I/O, error reporting
- **Server_Block_Parser**: Parses server blocks, creates ServerConfig instances
- **Location_Block_Parser**: Parses location blocks, creates LocationConfig instances
- **Config_Storage**: Stores all parsed configuration data

**Key Design Decisions**:
- Parsing order: Server blocks MUST be fully parsed before their location blocks
- Config_Storage is read-only after parsing completes (thread-safe for read access)
- All validation happens during parsing, not during request handling
- Parser uses line-by-line tokenization with context tracking for error reporting


#### 2. HTTP Server Core

**Responsibility**: Manage non-blocking I/O, connections, and virtual servers

**Sub-components**:

- **IO_Multiplexer**: Wraps poll()/select()/epoll()/kqueue() for platform abstraction
- **Connection_Manager**: Tracks all active Client_Connection instances
- **Virtual_Server_Manager**: Manages multiple Virtual_Server instances on different ports

**Key Design Decisions**:
- Single event loop architecture: one thread handles all I/O
- State machine per connection: tracks parsing state, response state
- Separate read/write buffers per connection to handle partial I/O
- Timeout tracking using timestamps, checked on each event loop iteration
- Platform-specific I/O multiplexing selected at compile time

**Connection State Machine**:
```
ACCEPTING → READING_REQUEST → PROCESSING → WRITING_RESPONSE → CLOSING
     ↑                                                            │
     └────────────────────────────────────────────────────────────┘
                    (connection reuse for keep-alive)
```

#### 3. Request Handler Subsystem

**Responsibility**: Process HTTP requests and generate responses

**Sub-components**:

- **Request_Handler**: Orchestrates request processing, applies configuration
- **HTTP_Parser**: Parses HTTP request messages (method, URI, headers, body)
- **Response_Builder**: Constructs HTTP response messages
- **CGI_Executor**: Executes CGI scripts and captures output

**Key Design Decisions**:
- Request_Handler receives const Config_Storage& in constructor
- All request processing is stateless (no shared mutable state)
- CGI execution uses fork() + exec() with pipe-based I/O
- File operations use non-blocking I/O where possible
- MIME type detection based on file extension lookup table

## Components and Interfaces

### Config_Parser Interface

```cpp
class Config_Parser {
public:
    Config_Parser();
    
    // Parse configuration file and populate storage
    // Returns true on success, false on error
    bool parseFile(const std::string& filepath);
    
    // Get parsed configuration (read-only access)
    const Config_Storage& getConfig() const;
    
    // Get last error message
    std::string getLastError() const;
    
private:
    Config_Storage storage_;
    Server_Block_Parser server_parser_;
    Location_Block_Parser location_parser_;
    std::string last_error_;
    int current_line_;
};
```

### Server_Block_Parser Interface

```cpp
class Server_Block_Parser {
public:
    // Parse a server block from token stream
    // Returns ServerConfig on success, throws on error
    ServerConfig parseServerBlock(std::vector<std::string>& tokens, int& line_num);
    
private:
    void parseListen(ServerConfig& config, const std::string& value);
    void parseServerName(ServerConfig& config, const std::string& value);
    void parseHost(ServerConfig& config, const std::string& value);
    void parseRoot(ServerConfig& config, const std::string& value);
    void parseIndex(ServerConfig& config, const std::string& value);
    void parseErrorPage(ServerConfig& config, const std::string& value);
    void parseClientMaxBodySize(ServerConfig& config, const std::string& value);
};
```


### Location_Block_Parser Interface

```cpp
class Location_Block_Parser {
public:
    // Parse a location block from token stream
    // Returns LocationConfig on success, throws on error
    LocationConfig parseLocationBlock(std::vector<std::string>& tokens, int& line_num);
    
private:
    void parseAllowedMethods(LocationConfig& config, const std::string& value);
    void parseAutoindex(LocationConfig& config, const std::string& value);
    void parseReturn(LocationConfig& config, const std::string& value);
    void parseCgiPath(LocationConfig& config, const std::string& value);
    void parseCgiExtension(LocationConfig& config, const std::string& value);
    void parseUploadDir(LocationConfig& config, const std::string& value);
};
```

### Config_Storage Interface

```cpp
class Config_Storage {
public:
    Config_Storage();
    
    // Add a server configuration
    void addServer(const ServerConfig& server);
    
    // Get server config matching host:port
    // Returns NULL if no match found
    const ServerConfig* getServerConfig(const std::string& host, int port) const;
    
    // Get location config for a path within a server
    // Uses longest prefix matching on Location_Map
    // Returns NULL if no match found
    const LocationConfig* getLocationConfig(const ServerConfig* server, 
                                           const std::string& path) const;
    
    // Get all servers (for initialization)
    const std::vector<ServerConfig>& getServers() const;
    
private:
    std::vector<ServerConfig> servers_;
    
    // Helper: Find longest matching location path
    std::string findLongestMatch(const std::map<std::string, LocationConfig>& locations,
                                 const std::string& path) const;
};
```

### Request_Handler Interface

```cpp
class Request_Handler {
public:
    // Constructor receives configuration
    Request_Handler(const Config_Storage& config);
    
    // Process a complete HTTP request and generate response
    // Returns response string ready to send
    std::string handleRequest(const std::string& request_data, 
                             const ServerConfig* server_config);
    
private:
    const Config_Storage& config_;
    HTTP_Parser parser_;
    Response_Builder builder_;
    CGI_Executor cgi_executor_;
    
    // Request processing pipeline
    std::string handleGet(const HttpRequest& req, const ServerConfig* srv, 
                         const LocationConfig* loc);
    std::string handlePost(const HttpRequest& req, const ServerConfig* srv, 
                          const LocationConfig* loc);
    std::string handleDelete(const HttpRequest& req, const ServerConfig* srv, 
                            const LocationConfig* loc);
    
    // Helper methods
    bool isMethodAllowed(const std::string& method, const LocationConfig* loc);
    std::string serveStaticFile(const std::string& path, const ServerConfig* srv);
    std::string generateDirectoryListing(const std::string& path);
    std::string handleFileUpload(const HttpRequest& req, const LocationConfig* loc);
    std::string executeCgi(const HttpRequest& req, const LocationConfig* loc);
    std::string buildErrorResponse(int status_code, const ServerConfig* srv);
};
```

### HTTP_Parser Interface

```cpp
struct HttpRequest {
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

class HTTP_Parser {
public:
    // Parse HTTP request from buffer
    // Returns true if complete request parsed, false if incomplete
    bool parse(const std::string& buffer, HttpRequest& request);
    
    // Check if parser has encountered an error
    bool hasError() const;
    
    // Get error message
    std::string getError() const;
    
private:
    enum ParseState { PARSE_METHOD, PARSE_URI, PARSE_VERSION, 
                     PARSE_HEADERS, PARSE_BODY, PARSE_COMPLETE };
    ParseState state_;
    std::string error_;
    
    bool parseRequestLine(const std::string& line, HttpRequest& req);
    bool parseHeader(const std::string& line, HttpRequest& req);
};
```


### Response_Builder Interface

```cpp
class Response_Builder {
public:
    Response_Builder();
    
    // Build complete HTTP response
    std::string buildResponse(int status_code, 
                             const std::map<std::string, std::string>& headers,
                             const std::string& body);
    
    // Build response with file content
    std::string buildFileResponse(const std::string& filepath);
    
    // Build error response
    std::string buildErrorResponse(int status_code, const std::string& message);
    
    // Build redirect response
    std::string buildRedirectResponse(int status_code, const std::string& location);
    
private:
    std::string getReasonPhrase(int status_code);
    std::string getMimeType(const std::string& filepath);
    std::string getCurrentDate();
};
```

### CGI_Executor Interface

```cpp
class CGI_Executor {
public:
    CGI_Executor();
    
    // Execute CGI script and return output
    // Returns empty string on error
    std::string execute(const std::string& script_path,
                       const std::string& interpreter_path,
                       const HttpRequest& request,
                       const ServerConfig* server);
    
    // Check if last execution had an error
    bool hasError() const;
    
    // Get error message
    std::string getError() const;
    
private:
    std::string error_;
    
    // Build CGI environment variables
    std::vector<std::string> buildEnvironment(const HttpRequest& request,
                                              const ServerConfig* server);
    
    // Execute script using fork/exec
    std::string executeScript(const std::string& interpreter,
                             const std::string& script,
                             const std::vector<std::string>& env,
                             const std::string& stdin_data);
};
```

### IO_Multiplexer Interface

```cpp
struct SocketEvent {
    int fd;
    bool readable;
    bool writable;
    bool error;
};

class IO_Multiplexer {
public:
    IO_Multiplexer();
    ~IO_Multiplexer();
    
    // Add socket to monitoring
    void addSocket(int fd, bool monitor_read, bool monitor_write);
    
    // Remove socket from monitoring
    void removeSocket(int fd);
    
    // Update monitoring flags for socket
    void updateSocket(int fd, bool monitor_read, bool monitor_write);
    
    // Wait for events (blocking with timeout)
    // Returns vector of ready sockets
    std::vector<SocketEvent> wait(int timeout_ms);
    
private:
    // Platform-specific implementation
#ifdef __linux__
    int epoll_fd_;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    int kqueue_fd_;
#else
    fd_set read_fds_;
    fd_set write_fds_;
    int max_fd_;
#endif
};
```

### Client_Connection Interface

```cpp
class Client_Connection {
public:
    enum State { READING_REQUEST, PROCESSING, WRITING_RESPONSE, CLOSING };
    
    Client_Connection(int socket_fd, const ServerConfig* server);
    
    // Get connection socket
    int getSocket() const;
    
    // Get current state
    State getState() const;
    
    // Handle read event
    void handleRead();
    
    // Handle write event
    void handleWrite();
    
    // Check if connection should be closed
    bool shouldClose() const;
    
    // Get last activity timestamp
    time_t getLastActivity() const;
    
    // Update last activity timestamp
    void updateActivity();
    
private:
    int socket_fd_;
    State state_;
    const ServerConfig* server_config_;
    std::string read_buffer_;
    std::string write_buffer_;
    size_t write_offset_;
    time_t last_activity_;
    HttpRequest current_request_;
};
```


## Data Models

### Configuration Data Structures (C++98 Style)

```cpp
// ServerConfig: Complete server block configuration
struct ServerConfig {
    // Network settings
    std::vector<int> listen_ports;              // Ports to listen on
    std::string host;                            // Binding address (default: 0.0.0.0)
    std::vector<std::string> server_names;       // Virtual server names
    
    // File serving settings
    std::string root;                            // Document root path
    std::vector<std::string> index_files;        // Default index files
    
    // Error handling
    std::map<int, std::string> error_pages;      // status_code -> file_path
    
    // Request limits
    size_t client_max_body_size;                 // Max request body size (bytes)
    
    // Location configurations (Location_Map)
    std::map<std::string, LocationConfig> locations;  // path -> config
    
    // Constructor with defaults
    ServerConfig() : host("0.0.0.0"), root("/var/www/html"), 
                     client_max_body_size(1048576) {}  // 1MB default
};

// LocationConfig: Location block configuration
struct LocationConfig {
    std::string path;                            // Location path (e.g., "/api")
    
    // Method restrictions
    std::vector<std::string> allowed_methods;    // Permitted HTTP methods
    
    // Directory listing
    bool autoindex;                              // Enable directory listing
    
    // Redirection
    std::string return_code;                     // HTTP redirect status code
    std::string return_url;                      // Redirect target URL
    
    // CGI settings
    std::string cgi_path;                        // Path to CGI interpreter
    std::vector<std::string> cgi_extensions;     // Extensions that trigger CGI
    
    // File upload
    std::string upload_dir;                      // Upload destination directory
    
    // Override server settings
    std::string root;                            // Override document root
    std::vector<std::string> index_files;        // Override index files
    
    // Constructor with defaults
    LocationConfig() : autoindex(false) {
        allowed_methods.push_back("GET");
        allowed_methods.push_back("POST");
        allowed_methods.push_back("DELETE");
    }
};

// HttpRequest: Parsed HTTP request
struct HttpRequest {
    std::string method;                          // HTTP method (GET, POST, DELETE)
    std::string uri;                             // Request URI
    std::string version;                         // HTTP version (HTTP/1.1)
    std::map<std::string, std::string> headers;  // Header name -> value
    std::string body;                            // Request body
    
    // Helper methods
    bool hasHeader(const std::string& name) const {
        return headers.find(name) != headers.end();
    }
    
    std::string getHeader(const std::string& name) const {
        std::map<std::string, std::string>::const_iterator it = headers.find(name);
        return (it != headers.end()) ? it->second : "";
    }
};

// SocketEvent: I/O multiplexer event
struct SocketEvent {
    int fd;                                      // File descriptor
    bool readable;                               // Socket ready for reading
    bool writable;                               // Socket ready for writing
    bool error;                                  // Socket has error condition
};
```

### Connection State Management

```cpp
// Connection states for state machine
enum ConnectionState {
    STATE_ACCEPTING,        // Accepting new connection
    STATE_READING_REQUEST,  // Reading HTTP request
    STATE_PROCESSING,       // Processing request (may involve file I/O, CGI)
    STATE_WRITING_RESPONSE, // Writing HTTP response
    STATE_CLOSING           // Closing connection
};

// Connection context
struct ConnectionContext {
    int socket_fd;
    ConnectionState state;
    const ServerConfig* server_config;
    std::string read_buffer;
    std::string write_buffer;
    size_t write_offset;
    time_t last_activity;
    HttpRequest request;
    bool keep_alive;
};
```


## Workflow Diagrams

### Configuration Parsing Sequence

```
User                Config_Parser       Server_Block_Parser    Location_Block_Parser    Config_Storage
 │                       │                      │                       │                      │
 │──parseFile()────────→ │                      │                       │                      │
 │                       │──open file           │                       │                      │
 │                       │──read lines          │                       │                      │
 │                       │                      │                       │                      │
 │                       │──parseServerBlock()→ │                       │                      │
 │                       │                      │──parse listen         │                      │
 │                       │                      │──parse server_name    │                      │
 │                       │                      │──parse root           │                      │
 │                       │                      │──parse index          │                      │
 │                       │                      │──parse error_page     │                      │
 │                       │                      │──parse client_max_body_size                  │
 │                       │                      │                       │                      │
 │                       │                      │←─ServerConfig─────────│                      │
 │                       │                      │                       │                      │
 │                       │──parseLocationBlock()──────────────────────→ │                      │
 │                       │                      │                       │──parse allowed_methods
 │                       │                      │                       │──parse autoindex     │
 │                       │                      │                       │──parse return        │
 │                       │                      │                       │──parse cgi_path      │
 │                       │                      │                       │──parse cgi_extension │
 │                       │                      │                       │──parse upload_dir    │
 │                       │                      │                       │                      │
 │                       │                      │                       │←─LocationConfig──────│
 │                       │                      │                       │                      │
 │                       │──addServer()─────────────────────────────────────────────────────→ │
 │                       │                      │                       │                      │
 │                       │                      │                       │                      │──store in vector
 │                       │                      │                       │                      │──store locations in map
 │                       │                      │                       │                      │
 │←─success──────────────│                      │                       │                      │
 │                       │                      │                       │                      │
```

### Request Processing Sequence

```
Client          IO_Multiplexer    Connection_Manager    Request_Handler    Config_Storage    Response_Builder
 │                    │                   │                    │                  │                 │
 │──HTTP Request─────→│                   │                    │                  │                 │
 │                    │──socket ready     │                    │                  │                 │
 │                    │──(read event)────→│                    │                  │                 │
 │                    │                   │──read()            │                  │                 │
 │                    │                   │──append to buffer  │                  │                 │
 │                    │                   │                    │                  │                 │
 │                    │                   │──handleRequest()──→│                  │                 │
 │                    │                   │                    │──getServerConfig()→                │
 │                    │                   │                    │                  │                 │
 │                    │                   │                    │←─ServerConfig────│                 │
 │                    │                   │                    │                  │                 │
 │                    │                   │                    │──getLocationConfig()               │
 │                    │                   │                    │                  │                 │
 │                    │                   │                    │←─LocationConfig──│                 │
 │                    │                   │                    │                  │                 │
 │                    │                   │                    │──check method allowed              │
 │                    │                   │                    │──serve static file                 │
 │                    │                   │                    │  OR execute CGI                    │
 │                    │                   │                    │  OR handle upload                  │
 │                    │                   │                    │                  │                 │
 │                    │                   │                    │──buildResponse()─────────────────→ │
 │                    │                   │                    │                  │                 │
 │                    │                   │                    │←─HTTP Response───────────────────── │
 │                    │                   │                    │                  │                 │
 │                    │                   │←─response string───│                  │                 │
 │                    │                   │                    │                  │                 │
 │                    │                   │──write()           │                  │                 │
 │                    │                   │                    │                  │                 │
 │←─HTTP Response─────────────────────────│                    │                  │                 │
 │                    │                   │                    │                  │                 │
```


### Connection State Machine

```
                    ┌─────────────────┐
                    │   ACCEPTING     │
                    │  (new socket)   │
                    └────────┬────────┘
                             │
                             │ accept()
                             ▼
                    ┌─────────────────┐
                    │ READING_REQUEST │◄──────┐
                    │  (recv data)    │       │
                    └────────┬────────┘       │
                             │                │
                             │ complete       │ incomplete
                             │ request        │ request
                             ▼                │
                    ┌─────────────────┐       │
                    │   PROCESSING    │       │
                    │ (handle request)│       │
                    └────────┬────────┘       │
                             │                │
                             │ response       │
                             │ ready          │
                             ▼                │
                    ┌─────────────────┐       │
                    │WRITING_RESPONSE │       │
                    │  (send data)    │       │
                    └────────┬────────┘       │
                             │                │
                    ┌────────┴────────┐       │
                    │                 │       │
              complete          incomplete    │
              write             write         │
                    │                 │       │
                    ▼                 └───────┘
            ┌───────────────┐
            │    CLOSING    │
            │ (close socket)│
            └───────────────┘
                    │
                    ▼
            (connection removed)
```

### CGI Execution Flow

```
Request_Handler      CGI_Executor         fork/exec          CGI Script
      │                   │                    │                  │
      │──execute()───────→│                    │                  │
      │                   │──fork()───────────→│                  │
      │                   │                    │                  │
      │                   │                    │──exec()─────────→│
      │                   │                    │                  │
      │                   │──write to stdin────────────────────→  │
      │                   │                    │                  │
      │                   │                    │                  │──process
      │                   │                    │                  │──generate output
      │                   │                    │                  │
      │                   │←──read from stdout─────────────────── │
      │                   │                    │                  │
      │                   │──waitpid()────────→│                  │
      │                   │                    │                  │
      │                   │←─exit status───────│                  │
      │                   │                    │                  │
      │←─CGI output───────│                    │                  │
      │                   │                    │                  │
```

## Error Handling

### Error Handling Strategy

**Principle**: The server must never crash. All errors are caught, logged, and handled gracefully.

**Error Categories**:

1. **Configuration Errors** (Fatal - prevent server start)
   - Invalid syntax in config file
   - Missing required directives
   - Invalid port numbers or file paths
   - Action: Log error with line number, exit with status code 1

2. **Network Errors** (Non-fatal - affect single connection)
   - Socket creation failure
   - Bind failure
   - Accept failure
   - Read/write errors
   - Action: Log error, close affected connection, continue serving others

3. **Request Processing Errors** (Non-fatal - return HTTP error)
   - Malformed HTTP requests
   - Method not allowed
   - File not found
   - Permission denied
   - Request body too large
   - Action: Send appropriate HTTP error response (4xx/5xx)

4. **CGI Execution Errors** (Non-fatal - return 500)
   - Script not found
   - Interpreter not found
   - Script execution timeout
   - Script crash
   - Action: Log error, return 500 Internal Server Error

5. **Resource Exhaustion** (Non-fatal - reject new connections)
   - Too many open connections
   - Out of memory
   - Disk full
   - Action: Log error, reject new connections until resources available


### Error Handling Implementation

```cpp
// Error handling in Config_Parser
bool Config_Parser::parseFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath.c_str());
        if (!file.is_open()) {
            last_error_ = "Cannot open file: " + filepath;
            return false;
        }
        
        // Parse file...
        
    } catch (const std::exception& e) {
        last_error_ = "Parse error at line " + intToString(current_line_) + 
                     ": " + e.what();
        return false;
    }
    return true;
}

// Error handling in Request_Handler
std::string Request_Handler::handleRequest(const std::string& request_data,
                                          const ServerConfig* server_config) {
    try {
        HttpRequest req;
        if (!parser_.parse(request_data, req)) {
            if (parser_.hasError()) {
                return builder_.buildErrorResponse(400, "Bad Request");
            }
            // Incomplete request - need more data
            return "";
        }
        
        // Validate request body size
        if (req.body.size() > server_config->client_max_body_size) {
            return builder_.buildErrorResponse(413, "Payload Too Large");
        }
        
        // Get location config
        const LocationConfig* loc = config_.getLocationConfig(server_config, req.uri);
        
        // Check method allowed
        if (!isMethodAllowed(req.method, loc)) {
            return builder_.buildErrorResponse(405, "Method Not Allowed");
        }
        
        // Route to appropriate handler
        if (req.method == "GET") {
            return handleGet(req, server_config, loc);
        } else if (req.method == "POST") {
            return handlePost(req, server_config, loc);
        } else if (req.method == "DELETE") {
            return handleDelete(req, server_config, loc);
        }
        
        return builder_.buildErrorResponse(501, "Not Implemented");
        
    } catch (const std::exception& e) {
        // Log error
        std::cerr << "Error handling request: " << e.what() << std::endl;
        return builder_.buildErrorResponse(500, "Internal Server Error");
    }
}

// Error handling in CGI_Executor
std::string CGI_Executor::execute(const std::string& script_path,
                                 const std::string& interpreter_path,
                                 const HttpRequest& request,
                                 const ServerConfig* server) {
    try {
        // Check script exists
        if (access(script_path.c_str(), F_OK) != 0) {
            error_ = "Script not found: " + script_path;
            return "";
        }
        
        // Check interpreter exists
        if (access(interpreter_path.c_str(), X_OK) != 0) {
            error_ = "Interpreter not found or not executable: " + interpreter_path;
            return "";
        }
        
        // Execute script with timeout
        std::string output = executeScript(interpreter_path, script_path, 
                                          buildEnvironment(request, server),
                                          request.body);
        
        return output;
        
    } catch (const std::exception& e) {
        error_ = "CGI execution failed: " + std::string(e.what());
        return "";
    }
}
```

### HTTP Error Response Mapping

```cpp
// Status code to reason phrase mapping
std::string Response_Builder::getReasonPhrase(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 307: return "Temporary Redirect";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown";
    }
}
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Configuration Parsing Round-Trip

*For any* valid configuration file containing server and location blocks, parsing the file and then querying the Config_Storage for the configured values should return values that match the original configuration directives.

**Validates: Requirements 1.1, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7**

### Property 2: Invalid Configuration Rejection

*For any* configuration file with syntax errors, invalid port numbers, missing required directives, or invalid file paths, the Config_Parser should reject the file and return an error message containing the line number and directive name.

**Validates: Requirements 1.5, 18.1, 18.2, 18.3, 18.4, 18.5, 18.6**

### Property 3: Configuration Retrieval

*For any* stored configuration, querying Config_Storage with a host and port should return the matching ServerConfig, and querying with a server and path should return the LocationConfig with the longest matching prefix.

**Validates: Requirements 3.9**

### Property 4: HTTP Request Parsing

*For any* valid HTTP/1.1 request message, parsing the request should correctly extract the method, URI, version, headers, and body into the HttpRequest structure.

**Validates: Requirements 5.1, 5.6**

### Property 5: Unsupported Method Rejection

*For any* HTTP request with a method other than GET, POST, or DELETE, the Request_Handler should respond with 405 Method Not Allowed.

**Validates: Requirements 5.5**

### Property 6: Request Body Size Validation

*For any* HTTP request where the body size exceeds the configured client_max_body_size, the Request_Handler should respond with 413 Payload Too Large without processing the request.

**Validates: Requirements 5.7, 5.8**

### Property 7: Static File Serving

*For any* GET request targeting an existing readable file, the Request_Handler should respond with 200 OK, the file contents in the body, and appropriate Content-Type and Content-Length headers.

**Validates: Requirements 6.1, 6.2, 6.3, 6.4**

### Property 8: Index File Resolution

*For any* GET request targeting a directory with a configured index file that exists, the Request_Handler should serve the index file instead of the directory.

**Validates: Requirements 6.7**

### Property 9: Directory Listing Generation

*For any* GET request targeting a directory where autoindex is enabled, the Request_Handler should generate an HTML response containing clickable links for all files and subdirectories in that directory.

**Validates: Requirements 7.1, 7.2, 7.3**

### Property 10: File Upload Handling

*For any* POST request with multipart/form-data encoding containing file data, the Request_Handler should extract the file content, save it to the configured upload_dir, and respond with 201 Created.

**Validates: Requirements 8.1, 8.2, 8.5, 8.6**

### Property 11: CGI Environment Variables

*For any* request targeting a CGI script, the CGI_Executor should pass all request headers as environment variables according to the CGI/1.1 specification (e.g., HTTP_HOST, CONTENT_TYPE, REQUEST_METHOD).

**Validates: Requirements 9.2**

### Property 12: CGI Input/Output

*For any* CGI script execution, the request body should be passed to the script via stdin, and the script's stdout should be captured and returned as the HTTP response body.

**Validates: Requirements 9.3, 9.4, 9.5, 9.6**

### Property 13: CGI Extension Matching

*For any* request targeting a file with an extension in the configured cgi_extensions list, the Request_Handler should execute the file using the configured cgi_path interpreter rather than serving it as a static file.

**Validates: Requirements 9.1**

### Property 14: Virtual Server Creation

*For any* configuration with N unique listen directives, the HTTP_Server should create exactly N Virtual_Server instances, each bound to its configured host and port.

**Validates: Requirements 10.1, 10.2**

### Property 15: Virtual Server Routing

*For any* incoming request, the HTTP_Server should route it to the Virtual_Server whose listening socket accepted the connection.

**Validates: Requirements 10.3**

### Property 16: HTTP Redirection

*For any* request targeting a location with a return directive configured, the Request_Handler should respond with the specified redirect status code and a Location header containing the redirect target URL.

**Validates: Requirements 11.1, 11.2**

### Property 17: Custom Error Pages

*For any* error response where a custom error_page is configured for that status code, the Request_Handler should serve the custom error page file instead of generating a default error page.

**Validates: Requirements 12.1, 12.2**

### Property 18: Default Error Pages

*For any* error response where no custom error_page is configured, the Request_Handler should generate a default HTML error page containing the status code and reason phrase.

**Validates: Requirements 12.3, 12.4**

### Property 19: Timeout Configuration

*For any* configuration with a request_timeout directive, the parsed timeout value should be accessible and used for connection timeout management.

**Validates: Requirements 13.4**

### Property 20: Exception Handling

*For any* exception thrown during request processing, the HTTP_Server should catch the exception, log an error message, and respond with 500 Internal Server Error without terminating.

**Validates: Requirements 14.1, 14.3, 14.4**

### Property 21: Malformed Request Handling

*For any* malformed HTTP request (invalid syntax, missing required headers, etc.), the Request_Handler should respond with 400 Bad Request without crashing.

**Validates: Requirements 14.6**

### Property 22: Method Restriction Enforcement

*For any* request targeting a location with allowed_methods configured, if the request method is not in the allowed list, the Request_Handler should respond with 405 Method Not Allowed and include an Allow header listing the permitted methods.

**Validates: Requirements 15.1, 15.2, 15.3**

### Property 23: Default Method Allowance

*For any* location without an allowed_methods directive, the Request_Handler should allow GET, POST, and DELETE methods by default.

**Validates: Requirements 15.4**

### Property 24: HTTP Response Format

*For any* HTTP response generated by the Request_Handler, the response should include a Status-Line, Date header, Server header, and proper CRLF CRLF separation between headers and body.

**Validates: Requirements 19.1, 19.2, 19.3, 19.4, 19.7**

### Property 25: Response Content Headers

*For any* HTTP response with a body, the response should include Content-Length and Content-Type headers with correct values.

**Validates: Requirements 19.5, 19.6**

### Property 26: DELETE Method Execution

*For any* DELETE request targeting an existing file in a location where DELETE is allowed, the Request_Handler should delete the file from the filesystem and respond with 204 No Content.

**Validates: Requirements 20.1, 20.2, 20.5**


## Testing Strategy

### Dual Testing Approach

The Webserv HTTP Server will employ both unit testing and property-based testing to ensure comprehensive correctness validation:

**Unit Tests**: Verify specific examples, edge cases, and error conditions
- Specific configuration file examples (valid and invalid)
- Edge cases: non-existent files (404), permission denied (403), empty directories
- Error conditions: port already in use, non-writable upload directory, CGI script failures
- Integration points: Config_Storage interface, Request_Handler with Config_Storage
- Specific HTTP methods: GET, POST, DELETE examples
- Specific redirect codes: 301, 302, 307 examples

**Property-Based Tests**: Verify universal properties across all inputs
- Configuration parsing correctness across randomly generated configs
- HTTP request parsing across randomly generated valid requests
- Static file serving across randomly generated file paths and contents
- Method restriction enforcement across random method/location combinations
- Error response generation across random error conditions
- CGI execution across random request data

Both testing approaches are complementary and necessary:
- Unit tests catch concrete bugs in specific scenarios
- Property tests verify general correctness across the input space
- Together they provide comprehensive coverage

### Property-Based Testing Configuration

**Library Selection**: 
- C++: Use RapidCheck (https://github.com/emil-e/rapidcheck) for property-based testing
- RapidCheck provides QuickCheck-style property testing for C++

**Test Configuration**:
- Minimum 100 iterations per property test (due to randomization)
- Each property test must reference its design document property
- Tag format: `// Feature: webserv-http-server, Property N: [property text]`

**Example Property Test Structure**:

```cpp
#include <rapidcheck.h>

// Feature: webserv-http-server, Property 1: Configuration Parsing Round-Trip
TEST_CASE("Config parsing round-trip") {
    rc::check([](const ServerConfig& config) {
        // Generate config file from ServerConfig
        std::string config_text = generateConfigText(config);
        
        // Parse the config file
        Config_Parser parser;
        parser.parseFile(config_text);
        const Config_Storage& storage = parser.getConfig();
        
        // Retrieve parsed config
        const ServerConfig* parsed = storage.getServerConfig(
            config.host, 
            config.listen_ports[0]
        );
        
        // Verify round-trip: parsed config matches original
        RC_ASSERT(parsed != NULL);
        RC_ASSERT(parsed->host == config.host);
        RC_ASSERT(parsed->listen_ports == config.listen_ports);
        RC_ASSERT(parsed->root == config.root);
        // ... verify all fields
    });
}

// Feature: webserv-http-server, Property 4: HTTP Request Parsing
TEST_CASE("HTTP request parsing") {
    rc::check([](const std::string& method, const std::string& uri,
                 const std::map<std::string, std::string>& headers,
                 const std::string& body) {
        // Generate valid HTTP request
        std::string request = method + " " + uri + " HTTP/1.1\r\n";
        for (std::map<std::string, std::string>::const_iterator it = headers.begin();
             it != headers.end(); ++it) {
            request += it->first + ": " + it->second + "\r\n";
        }
        request += "\r\n" + body;
        
        // Parse request
        HTTP_Parser parser;
        HttpRequest parsed;
        bool success = parser.parse(request, parsed);
        
        // Verify parsing correctness
        RC_ASSERT(success);
        RC_ASSERT(parsed.method == method);
        RC_ASSERT(parsed.uri == uri);
        RC_ASSERT(parsed.body == body);
        // ... verify headers
    });
}
```

### Unit Test Examples

```cpp
// Edge case: File not found
TEST_CASE("GET request for non-existent file returns 404") {
    Config_Storage config = createTestConfig();
    Request_Handler handler(config);
    
    std::string request = "GET /nonexistent.html HTTP/1.1\r\n\r\n";
    std::string response = handler.handleRequest(request, config.getServers()[0]);
    
    REQUIRE(response.find("404 Not Found") != std::string::npos);
}

// Edge case: Permission denied
TEST_CASE("GET request for unreadable file returns 403") {
    Config_Storage config = createTestConfig();
    Request_Handler handler(config);
    
    // Create file with no read permissions
    createUnreadableFile("/var/www/html/forbidden.html");
    
    std::string request = "GET /forbidden.html HTTP/1.1\r\n\r\n";
    std::string response = handler.handleRequest(request, config.getServers()[0]);
    
    REQUIRE(response.find("403 Forbidden") != std::string::npos);
}

// Edge case: Autoindex disabled, no index file
TEST_CASE("GET request for directory with autoindex off returns 403") {
    Config_Storage config = createTestConfig();
    config.getServers()[0].locations["/"].autoindex = false;
    Request_Handler handler(config);
    
    std::string request = "GET / HTTP/1.1\r\n\r\n";
    std::string response = handler.handleRequest(request, config.getServers()[0]);
    
    REQUIRE(response.find("403 Forbidden") != std::string::npos);
}

// Specific example: CGI script execution failure
TEST_CASE("CGI script failure returns 500") {
    Config_Storage config = createTestConfig();
    Request_Handler handler(config);
    
    // Create CGI script that exits with error
    createFailingCgiScript("/var/www/cgi-bin/fail.php");
    
    std::string request = "GET /cgi-bin/fail.php HTTP/1.1\r\n\r\n";
    std::string response = handler.handleRequest(request, config.getServers()[0]);
    
    REQUIRE(response.find("500 Internal Server Error") != std::string::npos);
}

// Specific example: Port already in use
TEST_CASE("Server reports error when port is in use") {
    // Bind to port 8080
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 10);
    
    // Try to start server on same port
    Config_Storage config = createTestConfig();
    config.getServers()[0].listen_ports.push_back(8080);
    
    HTTP_Server server(config);
    bool started = server.start();
    
    REQUIRE(!started);
    REQUIRE(server.getLastError().find("port") != std::string::npos);
    
    close(sock);
}
```

### Test Organization

```
tests/
├── unit/
│   ├── test_config_parser.cpp
│   ├── test_server_block_parser.cpp
│   ├── test_location_block_parser.cpp
│   ├── test_http_parser.cpp
│   ├── test_request_handler.cpp
│   ├── test_response_builder.cpp
│   ├── test_cgi_executor.cpp
│   └── test_io_multiplexer.cpp
├── property/
│   ├── test_config_properties.cpp
│   ├── test_http_properties.cpp
│   ├── test_request_properties.cpp
│   └── test_error_properties.cpp
├── integration/
│   ├── test_full_server.cpp
│   └── test_virtual_servers.cpp
└── fixtures/
    ├── configs/
    ├── static_files/
    └── cgi_scripts/
```

### Memory Management Strategy

**Principle**: Avoid memory leaks and undefined behavior through careful resource management.

**Strategies**:

1. **RAII (Resource Acquisition Is Initialization)**
   - Wrap resources (sockets, file descriptors, memory) in classes
   - Constructors acquire resources, destructors release them
   - Ensures cleanup even when exceptions occur

2. **Smart Pointer Alternative (C++98)**
   - Use std::auto_ptr for single ownership (deprecated but available in C++98)
   - Prefer manual memory management with clear ownership rules
   - Document ownership in comments

3. **Container Management**
   - Use std::vector and std::map for automatic memory management
   - Avoid raw arrays where possible
   - Clear containers when no longer needed

4. **Socket Management**
   - Close all sockets in destructors
   - Track open sockets in a container
   - Close sockets on error paths

5. **File Descriptor Management**
   - Close file descriptors immediately after use
   - Use RAII wrappers for file operations
   - Ensure CGI pipes are closed in all code paths

**Example RAII Socket Wrapper**:

```cpp
class SocketGuard {
public:
    explicit SocketGuard(int fd) : fd_(fd) {}
    
    ~SocketGuard() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    
    int get() const { return fd_; }
    
    int release() {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }
    
private:
    int fd_;
    
    // Prevent copying
    SocketGuard(const SocketGuard&);
    SocketGuard& operator=(const SocketGuard&);
};
```

### Development Workflow

**Phase 1: Shared Data Structures (All Developers)**
- Define and agree on ServerConfig, LocationConfig, Config_Storage structures
- Create header files with complete structure definitions
- Write unit tests for Config_Storage interface
- Estimated time: 1 day

**Phase 2: Parallel Development**

Developer 1 (Server Block Parser):
- Implement Server_Block_Parser
- Write unit tests for each directive parser
- Write property tests for server block parsing
- Estimated time: 3-4 days

Developer 2 (Location Block Parser):
- Implement Location_Block_Parser (depends on ServerConfig from Dev 1)
- Write unit tests for each directive parser
- Write property tests for location block parsing
- Estimated time: 3-4 days

Developer 3 (Request Handler):
- Implement HTTP_Parser, Response_Builder, CGI_Executor
- Implement Request_Handler (uses Config_Storage interface)
- Write unit tests for each component
- Write property tests for request handling
- Estimated time: 5-6 days

**Phase 3: Integration**
- Integrate all components
- Implement HTTP_Server core with IO_Multiplexer
- Run integration tests
- Fix integration issues
- Estimated time: 2-3 days

**Phase 4: Testing and Refinement**
- Run full test suite
- Test with real HTTP clients (browsers, curl)
- Fix bugs and edge cases
- Performance testing
- Estimated time: 2-3 days

**Total Estimated Time**: 13-17 days for a 3-person team

