*This project has been created as part of the 42 curriculum by bherranz, miparis, jaime.*

## Description

webserv is an HTTP/1.1 server written in C++98 from scratch. It uses a single `poll()` event loop for all I/O, parses an nginx-style configuration file, serves static files, executes CGI scripts, and handles file uploads. The project is part of the 42 Madrid common core and explores the HTTP protocol, non-blocking socket programming, and process management.

### Features

- **HTTP/1.1** — GET, POST, DELETE methods; chunked transfer encoding; keep-alive connections
- **Configuration** — nginx-style config file with server blocks, locations, directives
- **Static files** — Serve files with automatic MIME type detection, directory listing (autoindex)
- **CGI/1.1** — Execute Python and Shell scripts via fork+execve with full environment variables, 5-second timeout, 504 Gateway Timeout on expiry
- **File upload** — POST body stored as file; configurable `upload_store` directory
- **Error pages** — Built-in error pages for 4xx/5xx; custom pages via `error_page` directive
- **Redirects** — 301/302/307 via `return` directive
- **Virtual hosts** — Route requests by `Host` header matching `server_name`
- **Timeouts** — Configurable `client_timeout` (default 60s) and `keepalive_timeout` (default 10s); 408 Request Timeout response
- **Non-blocking** — Single `poll()` for all client and listen socket I/O
- **C++98** — Fully compliant, compiles with `-std=c++98 -Wall -Wextra -Werror`

## Instructions

### Dependencies

- C++ compiler (g++ or clang++)
- Make
- Linux or macOS

### Build

```sh
git clone <repository-url>
cd webserv/webserv
make
```

### Run

```sh
./Webserv [configuration-file]
```

If no configuration file is provided, `web.config` is used by default.

### Configuration

The configuration file uses an nginx-like syntax:

```
server {
    listen 8080;
    server_name localhost;
    host 0.0.0.0;
    root ./www;
    index index.html;
    client_max_body_size 1000000;
    client_timeout 60;
    keepalive_timeout 10;

    location / {
        allow_methods GET POST DELETE;
        autoindex on;
    }

    location /cgi-bin {
        allow_methods GET POST DELETE;
        cgi_path /usr/bin/python3 /bin/bash;
        cgi_ext .py .sh;
    }

    location /uploads {
        allow_methods GET POST DELETE;
        upload_store ./www/uploads;
    }
}
```

#### Directives

| Directive | Context | Description |
|---|---|---|
| `listen` | server | Port to listen on |
| `server_name` | server | Virtual host name |
| `host` | server | IP address to bind |
| `root` | server/location | Document root directory |
| `index` | server/location | Default file for directory requests |
| `autoindex` | location | Enable directory listing (`on`/`off`) |
| `allow_methods` | location | Allowed HTTP methods (`GET POST DELETE`) |
| `client_max_body_size` | server/location | Maximum request body size in bytes |
| `client_timeout` | server | Max seconds to wait for a complete request |
| `keepalive_timeout` | server | Max seconds idle before closing keep-alive |
| `cgi_path` | location | CGI interpreter paths |
| `cgi_ext` | location | CGI file extensions |
| `return` | location | URL redirect (optional status code) |
| `error_page` | server | Custom error page (`code path`) |
| `upload_store` | location | Directory for uploaded files |

### Usage examples

```sh
# Serve static file
curl http://localhost:8080/index.html

# Upload a file
curl -X POST -d "hello world" http://localhost:8080/uploads/test.txt

# Delete a file
curl -X DELETE http://localhost:8080/cgi-bin/test.txt

# CGI
curl http://localhost:8080/cgi-bin/test.py

# Keep-alive
curl -H "Connection: keep-alive" http://localhost:8080/

# Chunked upload
printf "5\r\nHello\r\n0\r\n\r\n" | curl -X POST -H "Transfer-Encoding: chunked" \
  --data-binary @- http://localhost:8080/uploads/chunked.txt

# Directory listing
curl http://localhost:8080/uploads/

# Custom error page (server with error_page directive)
curl http://localhost:8080/nonexistent
```

## Resources

- [RFC 7230 — HTTP/1.1 Message Syntax and Routing](https://tools.ietf.org/html/rfc7230)
- [RFC 7231 — HTTP/1.1 Semantics and Content](https://tools.ietf.org/html/rfc7231)
- [RFC 3875 — CGI/1.1](https://tools.ietf.org/html/rfc3875)
- [NGINX documentation](https://nginx.org/en/docs/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

### AI use

AI (Claude via opencode) was used for:
- Code generation and refactoring (Server event loop, Router, CGI, Config parser)
- Debugging (body double-append bug, keep-alive timeout, valgrind analysis)
- Code review and C++98 compliance verification
- Test scripting
- Documentation and this README

All AI-generated code was reviewed, tested, and modified by the authors.
