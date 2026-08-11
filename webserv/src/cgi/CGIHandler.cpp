#include "CGIHandler.hpp"
#include "Utils.hpp"

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cctype>

// Constants for CGI environment variables
static const std::string CGI_VERSION = "GATEWAY_INTERFACE=CGI/1.1";
static const std::string SERVER_SOFTWARE = "SERVER_SOFTWARE=webserv/1.0";
static const std::string HTTP_PROTOCOL = "SERVER_PROTOCOL=HTTP/1.1";

CGIHandler::CGIHandler(const HttpRequest &req, const LocationConfig &loc, const ServerConfig &server)
	: _req(req), _loc(loc), _server(server) {}

CGIHandler::~CGIHandler() {}

std::string CGIHandler::resolveScriptPath() const {
	std::string root;
	std::string uri = _req._path;
	std::size_t qmark = uri.find('?');
	if (qmark != std::string::npos)
		uri = uri.substr(0, qmark);
	
	uri = Utils::urlDecode(uri);

	if (_loc.hasRoot && !_loc.root.empty()) {
		root = _loc.root;
		if (!root.empty() && root[root.size() - 1] == '/')
			root.erase(root.size() - 1);
	} else if (_server.hasRoot && !_server.root.empty()) {
		root = _server.root;
		if (!root.empty() && root[root.size() - 1] == '/')
			root.erase(root.size() - 1);
	} else {
		root = ".";
	}

	if (!uri.empty() && uri[0] == '/')
		uri = uri.substr(1);

	if (uri.empty())
		return root;
	return root + "/" + uri;
}

// Determines the interpreter to use based on the CGI configuration and the script extension
std::string CGIHandler::getInterpreter() const {
	if (_loc.cgiExt.empty() || _loc.cgiPath.empty())
		return ""; // no CGI configuration, return empty string
	
	// clean uri
	std::string uri = _req._path;
	std::size_t qmark = uri.find('?');
	if (qmark != std::string::npos)
		uri = uri.substr(0, qmark);
	
	uri = Utils::urlDecode(uri);

	std::size_t dot = uri.rfind('.');
	if (dot == std::string::npos)
		return ""; // no extension found

	std::string ext = uri.substr(dot);

	for (std::size_t i = 0; i < _loc.cgiExt.size(); ++i) {
		std::string configuredExt = _loc.cgiExt[i];
		if (!configuredExt.empty() && configuredExt[0] != '.')
			configuredExt = "." + configuredExt;
		if (configuredExt == ext) {
			if (i < _loc.cgiPath.size())
				return _loc.cgiPath[i];
			else if (!_loc.cgiPath.empty())
				return _loc.cgiPath[0];
		}
	}

	return ""; // no matching extension found
}

bool isCgiExtension(const std::string &path, const LocationConfig &loc) {
	if (loc.cgiExt.empty())
		return false; // no CGI configuration, return false
	
	// clean path
	std::string cleanPath = path;
	std::size_t qmark = cleanPath.find('?');
	if (qmark != std::string::npos)
		cleanPath = cleanPath.substr(0, qmark);
	
	cleanPath = Utils::urlDecode(cleanPath);

	std::size_t dot = cleanPath.rfind('.');
	if (dot == std::string::npos)
		return false; // no extension found
	
	std::string ext = cleanPath.substr(dot);
	for (std::size_t i = 0; i < loc.cgiExt.size(); ++i) {
		std::string configuredExt = loc.cgiExt[i];
		if (!configuredExt.empty() && configuredExt[0] != '.')
			configuredExt = "." + configuredExt;
		if (configuredExt == ext)
			return true; // matching extension found
	}
	return false; // no matching extension found
}

void CGIHandler::freeEnv(char **env) const
{
    if (env == NULL)
        return;
    for (std::size_t i = 0; env[i] != NULL; ++i)
        delete[] env[i];
    delete[] env;
}

char **CGIHandler::buildEnv() const
{
	std::vector<std::string> env;

	// Add standard CGI environment variables
	env.push_back(CGI_VERSION);
	env.push_back(SERVER_SOFTWARE);
	env.push_back(HTTP_PROTOCOL);

	std::string serverName = _server.serverNames.empty() ? "localhost" : _server.serverNames[0];
	env.push_back("SERVER_NAME=" + serverName);

	if (!_server.listens.empty()) {
		std::ostringstream portSs;
		portSs << _server.listens[0].port;
		env.push_back("SERVER_PORT=" + portSs.str());
	} else {
		env.push_back("SERVER_PORT=8080");
	}

	// Add request method
	env.push_back("REQUEST_METHOD=" + _req._method);

	// Add request URI and path info
	std::string uri = _req._path;
	std::size_t qmark = uri.find('?');
	std::string pathOnly = (qmark != std::string::npos) ? uri.substr(0, qmark) : uri;
	std::string queryString = (qmark != std::string::npos) ? uri.substr(qmark + 1) : "";
	std::string scriptResolved = resolveScriptPath();

	env.push_back("REQUEST_URI=" + _req._path);
	env.push_back("SCRIPT_NAME=" + pathOnly);
	env.push_back("SCRIPT_FILENAME=" + scriptResolved);
	env.push_back("PATH_INFO=" + pathOnly);
	env.push_back("PATH_TRANSLATED=" + scriptResolved);
	env.push_back("QUERY_STRING=" + queryString);
	env.push_back("REDIRECT_STATUS=200"); // Required by php-cgi

	std::string docRoot = _loc.hasRoot ? _loc.root : (_server.hasRoot ? _server.root : ".");
	env.push_back("DOCUMENT_ROOT=" + docRoot);
	env.push_back("REMOTE_ADDR=127.0.0.1");

	// Content-Type and Content-Length
	std::map<std::string, std::string>::const_iterator ct = _req._headers.find("content-type");
	if (ct != _req._headers.end())
		env.push_back("CONTENT_TYPE=" + ct->second);
	else
		env.push_back("CONTENT_TYPE=");

	std::map<std::string, std::string>::const_iterator cl = _req._headers.find("content-length");
	if (cl != _req._headers.end())
		env.push_back("CONTENT_LENGTH=" + cl->second);
	else if (!_req._body.empty()) {
		std::ostringstream lenSs;
		lenSs << _req._body.size();
		env.push_back("CONTENT_LENGTH=" + lenSs.str());
	} else {
		env.push_back("CONTENT_LENGTH=");
	}

	// Add HTTP headers as environment variables
	for (std::map<std::string, std::string>::const_iterator it = _req._headers.begin();
         it != _req._headers.end(); ++it)
    {
        std::string name = it->first;
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            name[i] = std::toupper(static_cast<unsigned char>(name[i]));
            if (name[i] == '-')
                name[i] = '_';
        }
        if (name != "CONTENT_TYPE" && name != "CONTENT_LENGTH")
            env.push_back("HTTP_" + name + "=" + it->second);
    }

	char **envArray = new char *[env.size() + 1];
	for (std::size_t i = 0; i < env.size(); ++i)
	{
		envArray[i] = new char[env[i].size() + 1];
		std::strcpy(envArray[i], env[i].c_str());
	}
	envArray[env.size()] = NULL; // Null-terminate the array
	return envArray;
}

CgiFds CGIHandler::start(HttpResponse & res)
{
	CgiFds fds = {-1, -1, -1};
	std::string scriptPath = resolveScriptPath();
	std::string interpreter = getInterpreter();
	// Check if the script exists and if the interpreter is valid
	if (interpreter.empty() || !Utils::fileExists(scriptPath)) {
		res.setStatus(404, "Not Found");
		std::string body = "<html><body><h1>404 Not Found</h1><p>CGI script not found</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return (fds);
	}
	// Create pipes for stdin and stdout
	int stdinPipe[2];
	int stdoutPipe[2];
	if (pipe(stdinPipe) < 0) {
		res.setStatus(500, "Internal Server Error");
		std::string body = "<html><body><h1>500 Internal Server Error</h1><p>pipe() failed</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return (fds);
	}
	if (pipe(stdoutPipe) < 0) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		res.setStatus(500, "Internal Server Error");
		std::string body = "<html><body><h1>500 Internal Server Error</h1><p>pipe() failed</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return (fds);
	}

	char **env = buildEnv();
	pid_t pid = fork();
	if (pid < 0) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);
		freeEnv(env);
		res.setStatus(500, "Internal Server Error");
		std::string body = "<html><body><h1>500 Internal Server Error</h1><p>fork() failed</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return (fds);
	}
	if (pid == 0) {
		// Child process
		executeChild(stdinPipe, stdoutPipe, scriptPath, interpreter, env);
	}
	// Parent process
	close(stdinPipe[0]); // Close read end of stdin pipe
	close(stdoutPipe[1]); // Close write end of stdout pipe
	freeEnv(env);
	fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK); // Set stdout pipe to non-blocking
	fcntl(stdinPipe[1], F_SETFL, O_NONBLOCK); // Set stdin pipe to non-blocking

	fds.pid = pid;
	fds.readFd = stdoutPipe[0];
	fds.writeFd = stdinPipe[1];
	return (fds);
}

void CGIHandler::executeChild(int stdinPipe[2], int stdoutPipe[2], const std::string &scriptPath, 
					  const std::string &interpreter, char **env) const
{
	close(stdinPipe[1]);
	close(stdoutPipe[0]);
	// Redirect stdin and stdout
	dup2(stdinPipe[0], STDIN_FILENO);
	dup2(stdoutPipe[1], STDOUT_FILENO);

	close(stdinPipe[0]);
	close(stdoutPipe[1]);

	for (int fd = 3; fd < 256; ++fd) {
		::close(fd);
	}

	std::string dir = scriptPath;
	std::string scriptFile = scriptPath;
	std::size_t slash = scriptPath.rfind('/');
	if (slash != std::string::npos) {
		dir = scriptPath.substr(0, slash);
		scriptFile = scriptPath.substr(slash + 1);
		if (chdir(dir.c_str()) < 0) {
			std::cerr << "Failed to change directory to " << dir << std::endl;
			std::exit(1);
		}
	}

	char *argv[3];
	if (interpreter == scriptPath || interpreter.empty()) {
		std::string execTarget = "./" + scriptFile;
		argv[0] = const_cast<char*>(execTarget.c_str());
		argv[1] = NULL;
		execve(execTarget.c_str(), argv, env);
	} else {
		argv[0] = const_cast<char*>(interpreter.c_str());
		argv[1] = const_cast<char*>(scriptFile.c_str());
		argv[2] = NULL;
		execve(interpreter.c_str(), argv, env);
	}
	std::exit(1);
}

bool CGIHandler::finalize(const std::string &raw, HttpResponse &res)
{
	if (raw.empty()) {
		res.setStatus(500, "Internal Server Error");
		res.setBody("<html><body><h1>500 Internal Server Error</h1><p>CGI script returned no output</p></body></html>");
		res.setContentType("text/html");
		return false;
	}

	std::size_t crlfPos = raw.find("\r\n\r\n");
	std::size_t lfPos = raw.find("\n\n");

	std::size_t headerEnd = std::string::npos;
	std::size_t separatorSize = 0;

	if (crlfPos != std::string::npos && lfPos != std::string::npos) {
		if (crlfPos < lfPos) {
			headerEnd = crlfPos;
			separatorSize = 4;
		} else {
			headerEnd = lfPos;
			separatorSize = 2;
		}
	} else if (crlfPos != std::string::npos) {
		headerEnd = crlfPos;
		separatorSize = 4;
	} else if (lfPos != std::string::npos) {
		headerEnd = lfPos;
		separatorSize = 2;
	}

	if (headerEnd == std::string::npos) {
		res.setBody(raw);
		res.setContentType("text/html");
		res.setContentLength(raw.size());
		res.setStatus(200, "OK");
		return true;
	}

	std::string headerSection = raw.substr(0, headerEnd);
	std::string bodySection = raw.substr(headerEnd + separatorSize);
	// Parse the headers and set them in the response
	parseCgiHeaders(headerSection, res);

	res.setBody(bodySection);
	res.setContentLength(bodySection.size());
	return true;
}

void CGIHandler::parseCgiHeaders(const std::string &headerSection, HttpResponse &res)
{
	std::istringstream headerStream(headerSection);
	std::string line;

	int statusCode = 200;
	std::string statusReason = "OK";
	bool hasStatus = false;
	bool hasContentType = false;
	std::string locationValue;

	// Parse each header line
	while (std::getline(headerStream, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r') 
			line.erase(line.size() - 1);
		
		if (line.empty()) 
			continue;
		// Split the line into key and value
		std::size_t colon = line.find(':');
		if (colon == std::string::npos) 
			continue;

		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		
		if (!value.empty() && value[0] == ' ') 
			value.erase(0, 1);

		std::string lowerKey = key;
		for (std::size_t i = 0; i < lowerKey.size(); ++i)
			lowerKey[i] = std::tolower(static_cast<unsigned char>(lowerKey[i]));

		if (lowerKey == "status") {
			std::istringstream statusSs(value);
			statusSs >> statusCode;
			if (statusSs) {
				std::getline(statusSs, statusReason);
				if (!statusReason.empty() && statusReason[0] == ' ') 
					statusReason.erase(0, 1);
				if (statusReason.empty())
					statusReason = HttpResponse::reasonPhrase(statusCode);
			}
			hasStatus = true;
		} 
		else if (lowerKey == "location") {
			locationValue = value;
			res.setHeader("Location", value);
		} 
		else if (lowerKey == "content-type") {
			res.setContentType(value);
			hasContentType = true;
		} 
		else {
			res.setHeader(key, value);
		}
	}

	if (!locationValue.empty() && !hasStatus) {
		statusCode = 302;
		statusReason = "Found";
		hasStatus = true;
	}

	if (!hasContentType && locationValue.empty())
		res.setContentType("text/html");

	// Final status
	if (hasStatus) 
		res.setStatus(statusCode, statusReason);
	else 
		res.setStatus(200, "OK");
}
