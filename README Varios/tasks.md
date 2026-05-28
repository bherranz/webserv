# Implementation Plan: Webserv HTTP Server

## Overview

This implementation plan follows a 3-person team division with clear component boundaries. The server will be implemented in C++98 with non-blocking I/O using poll(), serving static files, executing CGI scripts, and handling multiple concurrent connections. Implementation follows a simple-to-complex progression: basic socket server → static GET → config parsing → multiple ports → POST/DELETE → uploads → CGI → chunked encoding.

## Team Division

**Person 1 - Networking & Event Loop (Server Core)**: Socket creation, multiple ports, event loop with poll(), connection management, non-blocking I/O, client tracking

**Person 2 - HTTP Parser + Request/Response**: Parse HTTP requests (method, headers, body), handle GET/POST/DELETE, chunked encoding, build HTTP responses, status codes, error pages

**Person 3 - Configuration, Static Files & CGI**: Config file parser, static file serving, MIME types, directory listing, file uploads, CGI execution with fork/execve/pipe

## Tasks

### Phase 1: Shared Architecture & Data Structures (All 3 Developers)

- [ ] 1. Design shared data structures and interfaces
  - Define ServerConfig and LocationConfig structures in shared header
  - Define HttpRequest and HttpResponse structures
  - Define Config_Storage interface for accessing configuration
  - Define Request_Handler interface for processing requests
  - Agree on class responsibilities and boundaries
  - Create project directory structure (src/, include/, tests/)
  - _Requirements: 16.1, 16.2, 16.3, 16.4, 16.5, 17.1, 17.3, 17.4, 17.5_

- [ ]* 1.1 Write property test for shared data structures
  - **Property 1: Configuration Parsing Round-Trip**
  - **Validates: Requirements 1.1, 2.1-2.7, 3.2-3.7**

### Phase 2: Person 1 - Basic Server & Socket Management

- [ ] 2. Implement basic socket server (single port, blocking)
  - [ ] 2.1 Create Socket class for socket operations
    - Implement socket(), bind(), listen() wrappers
    - Implement RAII pattern for socket cleanup
    - Handle socket errors gracefully
    - _Requirements: 4.1, 4.6, 14.2, 14.3_
  
  - [ ] 2.2 Create Server class with single port support
    - Initialize socket on specified port
    - Implement accept() for new connections
    - Implement basic read/write operations
    - Test with telnet or netcat
    - _Requirements: 10.1, 10.2, 10.5_
  
  - [ ]* 2.3 Write unit tests for Socket class
    - Test socket creation and binding
    - Test error handling for invalid ports
    - Test cleanup on destruction

- [ ] 3. Checkpoint - Basic server accepts connections
  - Ensure server can bind to port and accept connections, ask the user if questions arise.

### Phase 3: Person 2 - Basic HTTP Parser

- [ ] 4. Implement HTTP request parser for GET requests
  - [ ] 4.1 Create HttpRequest structure
    - Define method, uri, version, headers, body fields
    - Implement helper methods (hasHeader, getHeader)
    - _Requirements: 5.1, 5.6_
  
  - [ ] 4.2 Create HTTP_Parser class
    - Parse request line (method, URI, HTTP version)
    - Parse headers (key: value format)
    - Handle CRLF line endings
    - Detect end of headers (empty line)
    - _Requirements: 5.1, 5.6_
  
  - [ ] 4.3 Implement request validation
    - Validate HTTP method (GET, POST, DELETE)
    - Validate HTTP version (HTTP/1.1)
    - Handle malformed requests gracefully
    - _Requirements: 5.5, 14.6_
  
  - [ ]* 4.4 Write property test for HTTP request parsing
    - **Property 4: HTTP Request Parsing**
    - **Validates: Requirements 5.1, 5.6**
  
  - [ ]* 4.5 Write unit tests for HTTP_Parser
    - Test valid GET request parsing
    - Test malformed request handling
    - Test missing headers

### Phase 4: Person 2 - HTTP Response Builder

- [ ] 5. Implement HTTP response builder
  - [ ] 5.1 Create HttpResponse structure
    - Define status_code, headers, body fields
    - _Requirements: 19.1, 19.2_
  
  - [ ] 5.2 Create Response_Builder class
    - Implement buildResponse() for complete responses
    - Generate Status-Line with status code and reason phrase
    - Add Date header with current timestamp
    - Add Server header identifying software
    - Add Content-Length and Content-Type headers
    - Format response with proper CRLF separators
    - _Requirements: 19.1, 19.2, 19.3, 19.4, 19.5, 19.6, 19.7_
  
  - [ ] 5.3 Implement error response generation
    - Create buildErrorResponse() method
    - Generate default HTML error pages
    - Support status codes: 400, 403, 404, 405, 408, 413, 500, 501, 502, 503, 504
    - _Requirements: 12.3, 12.4, 14.4_
  
  - [ ]* 5.4 Write property test for HTTP response format
    - **Property 24: HTTP Response Format**
    - **Validates: Requirements 19.1, 19.2, 19.3, 19.4, 19.7**
  
  - [ ]* 5.5 Write unit tests for Response_Builder
    - Test 200 OK response generation
    - Test error response generation
    - Test header formatting

### Phase 5: Person 3 - Static File Serving

- [ ] 6. Implement static file serving for GET requests
  - [ ] 6.1 Create FileServer class
    - Implement file existence checking
    - Implement file reading with error handling
    - Handle permission errors (403 Forbidden)
    - Handle missing files (404 Not Found)
    - _Requirements: 6.1, 6.5, 6.6_
  
  - [ ] 6.2 Implement MIME type detection
    - Create MIME type lookup table (extension → type)
    - Support common types: .html, .css, .js, .jpg, .png, .gif, .txt
    - Default to application/octet-stream for unknown types
    - _Requirements: 6.2, 6.3_
  
  - [ ] 6.3 Implement index file resolution
    - Check for index files when directory is requested
    - Support multiple index files (index.html, index.htm)
    - Serve first matching index file
    - _Requirements: 6.7_
  
  - [ ]* 6.4 Write property test for static file serving
    - **Property 7: Static File Serving**
    - **Validates: Requirements 6.1, 6.2, 6.3, 6.4**
  
  - [ ]* 6.5 Write unit tests for FileServer
    - Test existing file serving
    - Test 404 for missing files
    - Test 403 for permission denied
    - Test MIME type detection

### Phase 6: Integration - Simple GET Server

- [ ] 7. Integrate components for basic GET server
  - [ ] 7.1 Create Request_Handler class
    - Accept Config_Storage reference in constructor
    - Implement handleRequest() method
    - Route GET requests to FileServer
    - Use HTTP_Parser to parse requests
    - Use Response_Builder to generate responses
    - _Requirements: 16.4, 16.5_
  
  - [ ] 7.2 Wire Server, HTTP_Parser, Request_Handler, FileServer
    - Server accepts connection → reads request
    - HTTP_Parser parses request
    - Request_Handler routes to FileServer
    - Response_Builder generates response
    - Server sends response
    - _Requirements: 5.1, 5.2, 6.1_
  
  - [ ]* 7.3 Write integration tests for GET server
    - Test complete GET request flow
    - Test with real HTTP client (curl)

- [ ] 8. Checkpoint - Server serves static files via GET
  - Ensure all tests pass, ask the user if questions arise.


### Phase 7: Person 3 - Configuration File Parser (Server Blocks)

- [ ] 9. Implement configuration file parser for server blocks
  - [ ] 9.1 Create Config_Parser class
    - Implement file reading and line-by-line parsing
    - Track current line number for error reporting
    - Handle comments and whitespace
    - Detect server blocks and location blocks
    - _Requirements: 1.1, 1.5, 1.6_
  
  - [ ] 9.2 Create Server_Block_Parser class
    - Parse listen directive (port numbers)
    - Parse server_name directive (virtual server names)
    - Parse host directive (binding addresses)
    - Parse root directive (document root path)
    - Parse index directive (default index files)
    - Parse error_page directive (status code → file path)
    - Parse client_max_body_size directive (max body size)
    - Populate ServerConfig structure
    - _Requirements: 1.2, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8_
  
  - [ ] 9.3 Implement configuration validation
    - Validate port numbers (1-65535)
    - Validate file paths exist
    - Validate client_max_body_size is positive
    - Validate at least one listen directive per server
    - Report errors with line numbers
    - _Requirements: 18.1, 18.2, 18.3, 18.4, 18.5_
  
  - [ ]* 9.4 Write property test for configuration parsing
    - **Property 2: Invalid Configuration Rejection**
    - **Validates: Requirements 1.5, 18.1-18.6**
  
  - [ ]* 9.5 Write unit tests for Server_Block_Parser
    - Test valid server block parsing
    - Test invalid syntax detection
    - Test missing required directives
    - Test error message formatting

### Phase 8: Person 3 - Configuration File Parser (Location Blocks)

- [ ] 10. Implement location block parser
  - [ ] 10.1 Create Location_Block_Parser class
    - Parse location path
    - Parse allowed_methods directive (GET, POST, DELETE)
    - Parse autoindex directive (on/off)
    - Parse return directive (status code and URL)
    - Parse cgi_path directive (interpreter path)
    - Parse cgi_extension directive (file extensions)
    - Parse upload_dir directive (upload destination)
    - Populate LocationConfig structure
    - Store in Location_Map (std::map<std::string, LocationConfig>)
    - _Requirements: 1.3, 1.4, 1.7, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9_
  
  - [ ] 10.2 Implement Config_Storage class
    - Store vector of ServerConfig instances
    - Implement getServerConfig(host, port) with matching logic
    - Implement getLocationConfig(server, path) with longest prefix matching
    - Ensure read-only access after parsing
    - _Requirements: 1.4, 1.7, 3.9, 16.2, 16.3, 16.7_
  
  - [ ] 10.3 Validate location block syntax
    - Validate location paths start with "/"
    - Validate allowed_methods are valid HTTP methods
    - Validate cgi_path and upload_dir exist and are accessible
    - _Requirements: 18.6_
  
  - [ ]* 10.4 Write property test for location config retrieval
    - **Property 3: Configuration Retrieval**
    - **Validates: Requirements 3.9**
  
  - [ ]* 10.5 Write unit tests for Location_Block_Parser
    - Test valid location block parsing
    - Test longest prefix matching
    - Test location path validation

- [ ] 11. Checkpoint - Configuration parser complete
  - Ensure all tests pass, ask the user if questions arise.

### Phase 9: Person 1 - Multiple Port Support

- [ ] 12. Implement multiple port and virtual server support
  - [ ] 12.1 Create Virtual_Server class
    - Encapsulate socket, port, and server configuration
    - Implement separate accept() for each virtual server
    - Track which socket accepted which connection
    - _Requirements: 10.1, 10.2_
  
  - [ ] 12.2 Extend Server class for multiple ports
    - Create Virtual_Server instance for each listen directive
    - Bind each virtual server to its configured port
    - Handle port already in use errors
    - Route incoming connections to correct virtual server
    - _Requirements: 10.1, 10.2, 10.3, 10.5_
  
  - [ ]* 12.3 Write property test for virtual server creation
    - **Property 14: Virtual Server Creation**
    - **Validates: Requirements 10.1, 10.2**
  
  - [ ]* 12.4 Write unit tests for multiple ports
    - Test binding to multiple ports
    - Test port conflict detection
    - Test connection routing

### Phase 10: Person 1 - Non-Blocking I/O with poll()

- [ ] 13. Implement non-blocking I/O multiplexing
  - [ ] 13.1 Create IO_Multiplexer class
    - Implement poll() wrapper for cross-platform support
    - Add/remove/update socket monitoring
    - Implement wait() with timeout
    - Return vector of ready sockets (SocketEvent)
    - _Requirements: 4.1_
  
  - [ ] 13.2 Set sockets to non-blocking mode
    - Use fcntl() to set O_NONBLOCK flag
    - Handle EAGAIN/EWOULDBLOCK errors
    - _Requirements: 4.6_
  
  - [ ] 13.3 Create Client_Connection class
    - Track connection state (READING_REQUEST, PROCESSING, WRITING_RESPONSE, CLOSING)
    - Implement read buffer for partial request data
    - Implement write buffer for partial response data
    - Track last activity timestamp
    - Implement handleRead() and handleWrite() methods
    - _Requirements: 4.2, 4.4, 4.5_
  
  - [ ] 13.4 Implement Connection_Manager class
    - Track all active Client_Connection instances
    - Add/remove connections
    - Check for timeouts and close idle connections
    - _Requirements: 4.3, 13.1, 13.2, 13.3, 13.5_
  
  - [ ] 13.5 Integrate event loop with poll()
    - Main loop: poll() → process ready sockets → repeat
    - Handle read events: accept new connections or read data
    - Handle write events: send response data
    - Handle errors: close connections
    - Support 100+ concurrent connections
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7_
  
  - [ ]* 13.6 Write unit tests for IO_Multiplexer
    - Test socket monitoring
    - Test event detection
    - Test timeout handling
  
  - [ ]* 13.7 Write integration tests for concurrent connections
    - Test 100+ simultaneous connections
    - Test partial read/write handling

- [ ] 14. Checkpoint - Non-blocking server handles concurrent connections
  - Ensure all tests pass, ask the user if questions arise.

### Phase 11: Person 2 - POST Method & Request Body Parsing

- [ ] 15. Implement POST method support
  - [ ] 15.1 Extend HTTP_Parser for request body
    - Parse Content-Length header
    - Read request body based on Content-Length
    - Handle partial body reads (non-blocking)
    - Validate body size against client_max_body_size
    - _Requirements: 5.3, 5.7, 5.8_
  
  - [ ] 15.2 Implement body size validation
    - Check Content-Length against client_max_body_size
    - Return 413 Payload Too Large if exceeded
    - _Requirements: 5.7, 5.8_
  
  - [ ]* 15.3 Write property test for request body size validation
    - **Property 6: Request Body Size Validation**
    - **Validates: Requirements 5.7, 5.8**
  
  - [ ]* 15.4 Write unit tests for POST parsing
    - Test POST with body
    - Test Content-Length parsing
    - Test 413 for oversized body

### Phase 12: Person 3 - File Upload Handling

- [ ] 16. Implement file upload support
  - [ ] 16.1 Create UploadHandler class
    - Parse multipart/form-data encoding
    - Extract boundary from Content-Type header
    - Parse multipart parts (headers + content)
    - Extract filename from Content-Disposition header
    - _Requirements: 8.6_
  
  - [ ] 16.2 Implement file saving
    - Save uploaded files to configured upload_dir
    - Validate upload_dir is writable
    - Return 500 if upload_dir not writable
    - Return 201 Created on success
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_
  
  - [ ] 16.3 Integrate upload handling with POST
    - Detect multipart/form-data Content-Type
    - Route to UploadHandler
    - Generate appropriate response
    - _Requirements: 8.1_
  
  - [ ]* 16.4 Write property test for file upload handling
    - **Property 10: File Upload Handling**
    - **Validates: Requirements 8.1, 8.2, 8.5, 8.6**
  
  - [ ]* 16.5 Write unit tests for UploadHandler
    - Test multipart parsing
    - Test file saving
    - Test upload_dir validation
    - Test 500 for non-writable directory

### Phase 13: Person 2 - DELETE Method

- [ ] 17. Implement DELETE method support
  - [ ] 17.1 Extend Request_Handler for DELETE
    - Implement handleDelete() method
    - Check file exists
    - Check file permissions
    - Delete file using unlink()
    - Return 204 No Content on success
    - Return 404 if file not found
    - Return 403 if permission denied
    - _Requirements: 20.1, 20.2, 20.3, 20.4_
  
  - [ ] 17.2 Implement method restriction checking
    - Check allowed_methods for location
    - Return 405 Method Not Allowed if DELETE not allowed
    - Include Allow header with permitted methods
    - _Requirements: 15.1, 15.2, 15.3, 20.5_
  
  - [ ]* 17.3 Write property test for DELETE method
    - **Property 26: DELETE Method Execution**
    - **Validates: Requirements 20.1, 20.2, 20.5**
  
  - [ ]* 17.4 Write unit tests for DELETE
    - Test successful deletion
    - Test 404 for missing file
    - Test 403 for permission denied
    - Test 405 for disallowed method

### Phase 14: Person 3 - Directory Listing (Autoindex)

- [ ] 18. Implement directory listing support
  - [ ] 18.1 Create DirectoryLister class
    - Use opendir() and readdir() to list directory contents
    - Generate HTML page with file/directory links
    - Make links clickable (href attributes)
    - _Requirements: 7.1, 7.2, 7.3_
  
  - [ ] 18.2 Integrate autoindex with GET handler
    - Check if request targets directory
    - Check if autoindex is enabled for location
    - If enabled, generate directory listing
    - If disabled and no index file, return 403 Forbidden
    - _Requirements: 7.4_
  
  - [ ]* 18.3 Write property test for directory listing
    - **Property 9: Directory Listing Generation**
    - **Validates: Requirements 7.1, 7.2, 7.3**
  
  - [ ]* 18.4 Write unit tests for DirectoryLister
    - Test HTML generation
    - Test link formatting
    - Test 403 when autoindex disabled

### Phase 15: Person 3 - CGI Execution

- [ ] 19. Implement CGI script execution
  - [ ] 19.1 Create CGIExecutor class
    - Check if request targets CGI script (match cgi_extension)
    - Validate script file exists
    - Validate interpreter (cgi_path) exists and is executable
    - _Requirements: 9.1_
  
  - [ ] 19.2 Implement CGI environment variable building
    - Set REQUEST_METHOD, QUERY_STRING, CONTENT_TYPE, CONTENT_LENGTH
    - Convert HTTP headers to CGI format (HTTP_HOST, HTTP_USER_AGENT, etc.)
    - Set SCRIPT_FILENAME, PATH_INFO, SERVER_NAME, SERVER_PORT
    - Follow CGI/1.1 specification
    - _Requirements: 9.2_
  
  - [ ] 19.3 Implement CGI execution with fork/exec/pipe
    - Create pipes for stdin/stdout
    - Fork child process
    - In child: dup2() pipes to stdin/stdout, execve() interpreter
    - In parent: write request body to stdin pipe, read output from stdout pipe
    - Use waitpid() to get exit status
    - Handle script timeout (kill child if exceeds limit)
    - _Requirements: 9.3, 9.4, 9.5_
  
  - [ ] 19.4 Parse CGI response
    - Parse CGI headers (Content-Type, Status, Location, etc.)
    - Extract response body
    - Build HTTP response from CGI output
    - _Requirements: 9.5, 9.6_
  
  - [ ] 19.5 Implement CGI error handling
    - Return 500 if script not found
    - Return 500 if interpreter not found
    - Return 500 if script execution fails
    - Return 500 if script times out
    - Close all pipes in error paths
    - _Requirements: 9.7_
  
  - [ ] 19.6 Support PHP and Python CGI scripts
    - Test with .php extension → /usr/bin/php-cgi
    - Test with .py extension → /usr/bin/python
    - _Requirements: 9.8, 9.9_
  
  - [ ]* 19.7 Write property test for CGI environment variables
    - **Property 11: CGI Environment Variables**
    - **Validates: Requirements 9.2**
  
  - [ ]* 19.8 Write property test for CGI input/output
    - **Property 12: CGI Input/Output**
    - **Validates: Requirements 9.3, 9.4, 9.5, 9.6**
  
  - [ ]* 19.9 Write unit tests for CGIExecutor
    - Test environment variable building
    - Test script execution
    - Test stdin/stdout handling
    - Test error conditions

- [ ] 20. Checkpoint - CGI execution working
  - Ensure all tests pass, ask the user if questions arise.

### Phase 16: Person 3 - HTTP Redirection

- [ ] 21. Implement HTTP redirection support
  - [ ] 21.1 Extend Request_Handler for redirects
    - Check if location has return directive
    - Parse return status code and URL
    - Generate redirect response with Location header
    - Support 301, 302, 307 status codes
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_
  
  - [ ]* 21.2 Write property test for HTTP redirection
    - **Property 16: HTTP Redirection**
    - **Validates: Requirements 11.1, 11.2**
  
  - [ ]* 21.3 Write unit tests for redirects
    - Test 301 redirect
    - Test 302 redirect
    - Test 307 redirect
    - Test Location header

### Phase 17: Person 3 - Custom Error Pages

- [ ] 22. Implement custom error page support
  - [ ] 22.1 Extend Response_Builder for custom error pages
    - Check if error_page configured for status code
    - If configured, read and serve custom error page file
    - If not configured, generate default error page
    - Maintain correct status code in response
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5_
  
  - [ ]* 22.2 Write property test for custom error pages
    - **Property 17: Custom Error Pages**
    - **Validates: Requirements 12.1, 12.2**
  
  - [ ]* 22.3 Write unit tests for error pages
    - Test custom error page serving
    - Test default error page generation
    - Test status code preservation

### Phase 18: Person 1 - Request Timeout Management

- [ ] 23. Implement connection timeout handling
  - [ ] 23.1 Add timeout tracking to Client_Connection
    - Store last activity timestamp
    - Update timestamp on read/write
    - _Requirements: 13.1, 13.5_
  
  - [ ] 23.2 Implement timeout checking in event loop
    - Check all connections for timeout on each iteration
    - Close connections exceeding timeout
    - Send 408 Request Timeout before closing
    - _Requirements: 13.2, 13.3_
  
  - [ ] 23.3 Make timeout configurable
    - Add request_timeout directive to config parser
    - Use configured timeout value
    - _Requirements: 13.4_
  
  - [ ]* 23.4 Write unit tests for timeout handling
    - Test timeout detection
    - Test 408 response
    - Test connection closure

### Phase 19: Person 2 - Chunked Transfer Encoding

- [ ] 24. Implement chunked transfer encoding support
  - [ ] 24.1 Extend HTTP_Parser for chunked encoding
    - Detect Transfer-Encoding: chunked header
    - Parse chunk size (hexadecimal)
    - Read chunk data
    - Handle chunk extensions (ignore)
    - Detect final chunk (size 0)
    - Parse trailing headers
    - _Requirements: 5.1_
  
  - [ ] 24.2 Implement chunked request body assembly
    - Accumulate chunks into complete body
    - Validate chunk format
    - Handle malformed chunks gracefully
    - _Requirements: 5.1, 14.6_
  
  - [ ]* 24.3 Write unit tests for chunked encoding
    - Test valid chunked request
    - Test malformed chunks
    - Test final chunk detection

### Phase 20: Integration & Testing

- [ ] 25. Implement robust error handling
  - [ ] 25.1 Add exception handling to all components
    - Wrap request processing in try-catch
    - Catch all exceptions without terminating
    - Log errors to stderr
    - Return 500 Internal Server Error on exceptions
    - _Requirements: 14.1, 14.3, 14.4_
  
  - [ ] 25.2 Implement connection-level error isolation
    - Ensure errors in one connection don't affect others
    - Close only the affected connection
    - Continue serving other clients
    - _Requirements: 14.2_
  
  - [ ] 25.3 Add input validation throughout
    - Validate all user input before processing
    - Check array bounds
    - Validate file paths (prevent directory traversal)
    - _Requirements: 14.5_
  
  - [ ]* 25.4 Write property test for exception handling
    - **Property 20: Exception Handling**
    - **Validates: Requirements 14.1, 14.3, 14.4**
  
  - [ ]* 25.5 Write property test for malformed request handling
    - **Property 21: Malformed Request Handling**
    - **Validates: Requirements 14.6**

- [ ] 26. Implement method restriction enforcement
  - [ ] 26.1 Add method checking to Request_Handler
    - Check allowed_methods for location
    - Return 405 if method not allowed
    - Include Allow header with permitted methods
    - Default to GET, POST, DELETE if not configured
    - _Requirements: 15.1, 15.2, 15.3, 15.4_
  
  - [ ]* 26.2 Write property test for method restrictions
    - **Property 22: Method Restriction Enforcement**
    - **Validates: Requirements 15.1, 15.2, 15.3**
  
  - [ ]* 26.3 Write property test for default method allowance
    - **Property 23: Default Method Allowance**
    - **Validates: Requirements 15.4**

- [ ] 27. Final integration and testing
  - [ ] 27.1 Create comprehensive integration tests
    - Test complete request flow: socket → parse → route → handle → respond
    - Test all HTTP methods (GET, POST, DELETE)
    - Test static files, CGI, uploads, directory listing
    - Test error conditions and edge cases
    - Test concurrent connections
    - Test multiple virtual servers
  
  - [ ] 27.2 Test with real HTTP clients
    - Test with curl for all methods
    - Test with web browser for static content
    - Test with siege or ab for load testing
    - Test with malformed requests
  
  - [ ] 27.3 Memory leak testing
    - Run with valgrind to detect leaks
    - Fix all memory leaks
    - Ensure proper cleanup on shutdown
  
  - [ ] 27.4 Create example configuration files
    - Create basic config for static file serving
    - Create config with CGI support
    - Create config with multiple virtual servers
    - Create config with all features enabled

- [ ] 28. Final checkpoint - Complete server implementation
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional testing tasks and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Implementation follows simple-to-complex progression
- Property tests validate universal correctness properties (100+ iterations each)
- Unit tests validate specific examples and edge cases
- C++98 compliance required: no C++11 features, use std::map/vector/string
- Non-blocking I/O with poll() is critical for concurrent connection handling
- CGI execution is typically the most complex component
- All components must handle errors gracefully without crashing
