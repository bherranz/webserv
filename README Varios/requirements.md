# Requirements Document

## Introduction

The Webserv HTTP Server is a C++98 implementation of an HTTP/1.1 server designed for the 42 school curriculum. The system must handle multiple concurrent connections using non-blocking I/O, support configuration files similar to NGINX, serve static content, execute CGI scripts, and provide robust error handling. The project will be developed by a team of three developers with clearly defined component boundaries.

## Glossary

- **HTTP_Server**: The complete HTTP server system including all components
- **Config_Parser**: The component responsible for parsing configuration files
- **Server_Block_Parser**: The subcomponent that parses server-level configuration directives
- **Location_Block_Parser**: The subcomponent that parses location-level configuration directives
- **Request_Handler**: The component responsible for processing HTTP requests and generating responses
- **Config_Storage**: The shared data structure that stores parsed configuration data
- **Location_Map**: A map data structure (std::map) that stores location configurations keyed by path
- **IO_Multiplexer**: The non-blocking I/O system using poll(), select(), kqueue(), or epoll()
- **CGI_Executor**: The component responsible for executing CGI scripts
- **Virtual_Server**: A server instance listening on a specific host:port combination
- **Client_Connection**: A single TCP connection from a client to the server
- **Request_Timeout**: The maximum time allowed for a client to send a complete request
- **Max_Body_Size**: The maximum allowed size of an HTTP request body

## Requirements

### Requirement 1: Configuration File Parsing

**User Story:** As a server administrator, I want to define server behavior through a configuration file, so that I can customize the server without recompiling code.

#### Acceptance Criteria

1. WHEN a configuration file path is provided, THE Config_Parser SHALL read and parse the file
2. THE Server_Block_Parser SHALL parse server block directives before location block directives
3. THE Location_Block_Parser SHALL parse location blocks only after the parent server block is fully parsed
4. THE Config_Parser SHALL store all parsed configuration in Config_Storage using Location_Map for location blocks
5. IF the configuration file contains syntax errors, THEN THE Config_Parser SHALL report the error with line number and exit gracefully
6. IF the configuration file does not exist, THEN THE Config_Parser SHALL report the error and exit gracefully
7. THE Config_Storage SHALL use std::map<std::string, LocationConfig> for storing location configurations keyed by path

### Requirement 2: Server Block Configuration

**User Story:** As a server administrator, I want to configure server-level settings, so that I can control how the server behaves globally.

#### Acceptance Criteria

1. THE Server_Block_Parser SHALL parse the listen directive to extract port numbers
2. THE Server_Block_Parser SHALL parse the server_name directive to extract virtual server names
3. THE Server_Block_Parser SHALL parse the host directive to extract binding addresses
4. THE Server_Block_Parser SHALL parse the root directive to extract the document root path
5. THE Server_Block_Parser SHALL parse the index directive to extract default index file names
6. THE Server_Block_Parser SHALL parse the error_page directive to extract custom error page mappings
7. THE Server_Block_Parser SHALL parse the client_max_body_size directive to extract Max_Body_Size limits
8. THE Config_Storage SHALL store all server block configuration in a shared data structure accessible to all components

### Requirement 3: Location Block Configuration

**User Story:** As a server administrator, I want to configure path-specific behavior, so that different URLs can have different handling rules.

#### Acceptance Criteria

1. THE Location_Block_Parser SHALL parse location blocks only after the parent server block is complete
2. THE Location_Block_Parser SHALL parse the allowed_methods directive to extract permitted HTTP methods
3. THE Location_Block_Parser SHALL parse the autoindex directive to enable or disable directory listing
4. THE Location_Block_Parser SHALL parse the return directive to extract HTTP redirection rules
5. THE Location_Block_Parser SHALL parse the cgi_path directive to extract CGI interpreter paths
6. THE Location_Block_Parser SHALL parse the cgi_extension directive to extract file extensions that trigger CGI execution
7. THE Location_Block_Parser SHALL parse the upload_dir directive to extract file upload destination paths
8. THE Location_Block_Parser SHALL store all location configurations in Location_Map keyed by the location path
9. THE Config_Storage SHALL provide access to location configurations through the Location_Map interface

### Requirement 4: Non-Blocking I/O Multiplexing

**User Story:** As a server operator, I want the server to handle multiple concurrent connections efficiently, so that many clients can be served simultaneously without blocking.

#### Acceptance Criteria

1. THE IO_Multiplexer SHALL use poll(), select(), kqueue(), or epoll() for non-blocking I/O operations
2. THE IO_Multiplexer SHALL monitor all active Client_Connection sockets for read and write readiness
3. THE IO_Multiplexer SHALL monitor all listening sockets for new connection attempts
4. WHEN a socket becomes ready for reading, THE IO_Multiplexer SHALL process the read operation without blocking other connections
5. WHEN a socket becomes ready for writing, THE IO_Multiplexer SHALL process the write operation without blocking other connections
6. THE HTTP_Server SHALL never block on any I/O operation
7. THE HTTP_Server SHALL handle at least 100 concurrent Client_Connection instances

### Requirement 5: HTTP Request Processing

**User Story:** As a client, I want to send HTTP requests to the server, so that I can retrieve resources and interact with the web application.

#### Acceptance Criteria

1. THE Request_Handler SHALL parse HTTP/1.1 request messages according to RFC 2616
2. THE Request_Handler SHALL support the GET method for retrieving resources
3. THE Request_Handler SHALL support the POST method for submitting data
4. THE Request_Handler SHALL support the DELETE method for removing resources
5. WHEN a request uses an unsupported HTTP method, THE Request_Handler SHALL respond with 405 Method Not Allowed
6. THE Request_Handler SHALL extract request headers, method, URI, and body from the request message
7. THE Request_Handler SHALL validate that request body size does not exceed Max_Body_Size
8. IF request body size exceeds Max_Body_Size, THEN THE Request_Handler SHALL respond with 413 Payload Too Large

### Requirement 6: Static File Serving

**User Story:** As a client, I want to retrieve static files from the server, so that I can view web pages and download resources.

#### Acceptance Criteria

1. WHEN a GET request targets a static file, THE Request_Handler SHALL read the file from the document root
2. THE Request_Handler SHALL determine the MIME type based on file extension
3. THE Request_Handler SHALL include appropriate Content-Type headers in the response
4. THE Request_Handler SHALL include Content-Length headers in the response
5. IF the requested file does not exist, THEN THE Request_Handler SHALL respond with 404 Not Found
6. IF the requested file cannot be read due to permissions, THEN THE Request_Handler SHALL respond with 403 Forbidden
7. WHEN a GET request targets a directory and an index file is configured, THE Request_Handler SHALL serve the index file

### Requirement 7: Directory Listing (Autoindex)

**User Story:** As a client, I want to view directory contents when autoindex is enabled, so that I can browse available files.

#### Acceptance Criteria

1. WHEN a GET request targets a directory and autoindex is enabled for that location, THE Request_Handler SHALL generate an HTML directory listing
2. THE Request_Handler SHALL include all files and subdirectories in the listing
3. THE Request_Handler SHALL generate clickable links for each entry in the directory listing
4. WHEN autoindex is disabled and no index file exists, THE Request_Handler SHALL respond with 403 Forbidden

### Requirement 8: File Upload Handling

**User Story:** As a client, I want to upload files to the server, so that I can store content on the server.

#### Acceptance Criteria

1. WHEN a POST request contains file upload data, THE Request_Handler SHALL extract the file content
2. THE Request_Handler SHALL save uploaded files to the configured upload_dir for the matching location
3. THE Request_Handler SHALL validate that the upload_dir is writable
4. IF the upload_dir is not writable, THEN THE Request_Handler SHALL respond with 500 Internal Server Error
5. WHEN file upload succeeds, THE Request_Handler SHALL respond with 201 Created
6. THE Request_Handler SHALL handle multipart/form-data encoding for file uploads

### Requirement 9: CGI Script Execution

**User Story:** As a developer, I want to execute CGI scripts, so that I can generate dynamic content.

#### Acceptance Criteria

1. WHEN a request targets a file with a configured cgi_extension, THE CGI_Executor SHALL execute the script using the configured cgi_path interpreter
2. THE CGI_Executor SHALL pass request headers as environment variables according to CGI/1.1 specification
3. THE CGI_Executor SHALL pass request body to the CGI script via stdin
4. THE CGI_Executor SHALL capture CGI script output from stdout
5. THE CGI_Executor SHALL parse CGI response headers and body
6. THE Request_Handler SHALL send the CGI output as the HTTP response
7. IF the CGI script execution fails, THEN THE Request_Handler SHALL respond with 500 Internal Server Error
8. THE CGI_Executor SHALL support PHP scripts when configured
9. THE CGI_Executor SHALL support Python scripts when configured

### Requirement 10: Virtual Server Support

**User Story:** As a server administrator, I want to run multiple virtual servers on different ports, so that I can host multiple websites on one machine.

#### Acceptance Criteria

1. THE HTTP_Server SHALL create a Virtual_Server instance for each unique listen directive in the configuration
2. THE HTTP_Server SHALL bind each Virtual_Server to its configured host and port
3. WHEN a request arrives, THE HTTP_Server SHALL route it to the correct Virtual_Server based on the listening socket
4. THE HTTP_Server SHALL support at least 10 concurrent Virtual_Server instances
5. IF a port is already in use, THEN THE HTTP_Server SHALL report the error and exit gracefully

### Requirement 11: HTTP Redirection

**User Story:** As a server administrator, I want to redirect requests to different URLs, so that I can manage URL structure and external links.

#### Acceptance Criteria

1. WHEN a location has a return directive configured, THE Request_Handler SHALL respond with the specified HTTP redirect status code
2. THE Request_Handler SHALL include the Location header with the redirect target URL
3. THE Request_Handler SHALL support 301 Moved Permanently redirects
4. THE Request_Handler SHALL support 302 Found redirects
5. THE Request_Handler SHALL support 307 Temporary Redirect redirects

### Requirement 12: Error Page Handling

**User Story:** As a server administrator, I want to serve custom error pages, so that users see branded error messages.

#### Acceptance Criteria

1. WHEN an error response is generated, THE Request_Handler SHALL check for a custom error_page configuration for that status code
2. IF a custom error page is configured, THEN THE Request_Handler SHALL serve the custom error page file
3. IF no custom error page is configured, THEN THE Request_Handler SHALL generate a default error page
4. THE Request_Handler SHALL include the correct HTTP status code in error responses
5. THE Request_Handler SHALL support custom error pages for 400, 403, 404, 405, 413, 500, 501, 502, 503, and 504 status codes

### Requirement 13: Request Timeout Management

**User Story:** As a server operator, I want to close idle connections, so that server resources are not exhausted by slow or stalled clients.

#### Acceptance Criteria

1. THE HTTP_Server SHALL track the last activity time for each Client_Connection
2. WHEN a Client_Connection has no activity for Request_Timeout seconds, THE HTTP_Server SHALL close the connection
3. THE HTTP_Server SHALL respond with 408 Request Timeout before closing timed-out connections
4. THE Request_Timeout SHALL be configurable in the server configuration
5. THE HTTP_Server SHALL reset the activity timer when data is received from a Client_Connection

### Requirement 14: Robust Error Handling

**User Story:** As a server operator, I want the server to never crash, so that service remains available even when errors occur.

#### Acceptance Criteria

1. THE HTTP_Server SHALL catch and handle all exceptions without terminating
2. WHEN a Client_Connection causes an error, THE HTTP_Server SHALL close only that connection and continue serving other clients
3. THE HTTP_Server SHALL log all errors to stderr or a log file
4. THE HTTP_Server SHALL respond with 500 Internal Server Error when internal errors occur during request processing
5. THE HTTP_Server SHALL validate all input data before processing
6. THE HTTP_Server SHALL handle malformed HTTP requests without crashing

### Requirement 15: HTTP Method Restrictions

**User Story:** As a server administrator, I want to restrict HTTP methods per location, so that I can control what operations are allowed on different paths.

#### Acceptance Criteria

1. WHEN a request targets a location with allowed_methods configured, THE Request_Handler SHALL check if the request method is in the allowed list
2. IF the request method is not allowed for that location, THEN THE Request_Handler SHALL respond with 405 Method Not Allowed
3. THE Request_Handler SHALL include an Allow header listing permitted methods in 405 responses
4. WHERE no allowed_methods directive is configured, THE Request_Handler SHALL allow GET, POST, and DELETE methods by default

### Requirement 16: Component Interface Definitions

**User Story:** As a development team, we need clearly defined interfaces between components, so that three developers can work independently on different parts.

#### Acceptance Criteria

1. THE Config_Parser SHALL expose a getConfig() interface that returns a const reference to Config_Storage
2. THE Config_Storage SHALL expose a getServerConfig(host, port) interface that returns server-level configuration
3. THE Config_Storage SHALL expose a getLocationConfig(server, path) interface that returns location-level configuration using Location_Map
4. THE Request_Handler SHALL accept Config_Storage as a constructor parameter
5. THE Request_Handler SHALL expose a handleRequest(request, connection) interface for processing requests
6. THE Config_Parser SHALL complete all parsing before the HTTP_Server starts accepting connections
7. THE Config_Storage data structures SHALL be read-only after parsing is complete to ensure thread-safety

### Requirement 17: C++98 Compliance

**User Story:** As a 42 school student, I must use C++98 standard, so that the project meets academic requirements.

#### Acceptance Criteria

1. THE HTTP_Server SHALL compile with -std=c++98 flag
2. THE HTTP_Server SHALL not use C++11 or later features
3. THE HTTP_Server SHALL use std::map for Location_Map implementation
4. THE HTTP_Server SHALL use std::vector for dynamic arrays
5. THE HTTP_Server SHALL use std::string for string handling
6. THE HTTP_Server SHALL not use auto keyword, nullptr, or range-based for loops

### Requirement 18: Configuration File Format Validation

**User Story:** As a server administrator, I want clear error messages for configuration mistakes, so that I can quickly fix configuration problems.

#### Acceptance Criteria

1. THE Config_Parser SHALL validate that all server blocks have at least one listen directive
2. THE Config_Parser SHALL validate that file paths in root and error_page directives exist
3. THE Config_Parser SHALL validate that port numbers are in the valid range (1-65535)
4. THE Config_Parser SHALL validate that client_max_body_size values are positive integers
5. IF validation fails, THEN THE Config_Parser SHALL print a descriptive error message including the directive name and line number
6. THE Config_Parser SHALL validate that location paths start with a forward slash

### Requirement 19: HTTP Response Generation

**User Story:** As a client, I want to receive properly formatted HTTP responses, so that my browser can correctly interpret the server's response.

#### Acceptance Criteria

1. THE Request_Handler SHALL generate HTTP/1.1 compliant response messages
2. THE Request_Handler SHALL include a Status-Line with status code and reason phrase
3. THE Request_Handler SHALL include a Date header in all responses
4. THE Request_Handler SHALL include a Server header identifying the server software
5. THE Request_Handler SHALL include a Content-Length header for responses with a body
6. THE Request_Handler SHALL include a Content-Type header for responses with a body
7. THE Request_Handler SHALL separate headers from body with a blank line (CRLF CRLF)

### Requirement 20: DELETE Method Implementation

**User Story:** As a client, I want to delete resources using the DELETE method, so that I can remove files from the server.

#### Acceptance Criteria

1. WHEN a DELETE request targets an existing file, THE Request_Handler SHALL delete the file from the filesystem
2. WHEN file deletion succeeds, THE Request_Handler SHALL respond with 204 No Content
3. IF the file does not exist, THEN THE Request_Handler SHALL respond with 404 Not Found
4. IF the file cannot be deleted due to permissions, THEN THE Request_Handler SHALL respond with 403 Forbidden
5. THE Request_Handler SHALL only allow DELETE operations in locations where DELETE is in the allowed_methods list
