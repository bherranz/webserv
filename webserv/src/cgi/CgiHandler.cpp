#include "CGIHandler.hpp"
#include "Utils.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <algorithm>

CGIHandler::CGIHandler(const HttpRequest &req, const LocationConfig &loc, const ServerConfig &server)
	: _req(req), _loc(loc), _server(server) {}

CGIHandler::~CGIHandler() {}

std::string CGIHandler::resolveScriptPath() const
{
	std::string root;
	if (_loc.hasRoot && !_loc.root.empty())
		root = _loc.root;
	else if (_server.hasRoot && !_server.root.empty())
		root = _server.root;
	else
		root = ".";

	std::string uri = _req._path;
	std::size_t qmark = uri.find('?');
	if (qmark != std::string::npos)
		uri = uri.substr(0, qmark);

	if (!uri.empty() && uri[0] == '/')
		uri = uri.substr(1);

	if (!root.empty() && root[root.size() - 1] == '/')
		return root + uri;
	return root + "/" + uri;
}

std::string CGIHandler::getInterpreter() const
{
	if (_loc.cgiExt.empty() || _loc.cgiPath.empty())
		return "";

	std::string uri = _req._path;
	std::size_t qmark = uri.find('?');
	if (qmark != std::string::npos)
		uri = uri.substr(0, qmark);

	std::size_t dot = uri.rfind('.');
	if (dot == std::string::npos)
		return "";

	std::string ext = uri.substr(dot);
	for (std::size_t i = 0; i < _loc.cgiExt.size(); ++i)
	{
		if (_loc.cgiExt[i] == ext && i < _loc.cgiPath.size())
			return _loc.cgiPath[i];
	}

	return "";
}

std::string CGIHandler::envEscape(const std::string &str)
{
	return str;
}

char **CGIHandler::buildEnv() const
{
	std::vector<std::string> envStrings;

	envStrings.push_back("SERVER_SOFTWARE=webserv/1.0");
	envStrings.push_back("SERVER_NAME=" + (_server.serverNames.empty() ? "localhost" : _server.serverNames[0]));
	envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");
	envStrings.push_back("SERVER_PROTOCOL=HTTP/1.1");

	std::ostringstream portSs;
	if (!_server.listens.empty())
		portSs << _server.listens[0].port;
	else
		portSs << "80";
	envStrings.push_back("SERVER_PORT=" + portSs.str());
	envStrings.push_back("REQUEST_METHOD=" + _req._method);

	std::string uri = _req._path;
	std::size_t qmark = uri.find('?');
	std::string pathOnly = (qmark != std::string::npos) ? uri.substr(0, qmark) : uri;
	std::string queryString = (qmark != std::string::npos) ? uri.substr(qmark + 1) : "";

	envStrings.push_back("SCRIPT_NAME=" + pathOnly);
	envStrings.push_back("SCRIPT_FILENAME=" + resolveScriptPath());
	envStrings.push_back("PATH_INFO=" + pathOnly);
	envStrings.push_back("PATH_TRANSLATED=" + resolveScriptPath());

	if (!queryString.empty())
		envStrings.push_back("QUERY_STRING=" + queryString);
	else
		envStrings.push_back("QUERY_STRING=");

	std::map<std::string, std::string>::const_iterator ct = _req._headers.find("Content-Type");
	if (ct != _req._headers.end())
		envStrings.push_back("CONTENT_TYPE=" + ct->second);

	std::map<std::string, std::string>::const_iterator cl = _req._headers.find("Content-Length");
	if (cl != _req._headers.end())
		envStrings.push_back("CONTENT_LENGTH=" + cl->second);

	for (std::map<std::string, std::string>::const_iterator it = _req._headers.begin();
		 it != _req._headers.end(); ++it)
	{
		std::string name = it->first;
		std::transform(name.begin(), name.end(), name.begin(), ::toupper);
		for (std::size_t i = 0; i < name.size(); ++i)
		{
			if (name[i] == '-')
				name[i] = '_';
		}
		if (name != "CONTENT_TYPE" && name != "CONTENT_LENGTH")
			envStrings.push_back("HTTP_" + name + "=" + it->second);
	}

	char **env = new char*[envStrings.size() + 1];
	for (std::size_t i = 0; i < envStrings.size(); ++i)
	{
		env[i] = new char[envStrings[i].size() + 1];
		std::strcpy(env[i], envStrings[i].c_str());
	}
	env[envStrings.size()] = NULL;

	return env;
}

namespace {
	void freeEnv(char **env)
	{
		if (env == NULL)
			return;
		for (std::size_t i = 0; env[i] != NULL; ++i)
			delete[] env[i];
		delete[] env;
	}
}

bool CGIHandler::parseOutput(const std::string &raw, HttpResponse &res)
{
	if (raw.empty())
		return false;

	std::size_t headerEnd = raw.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
	{
		std::size_t headerEndLf = raw.find("\n\n");
		if (headerEndLf == std::string::npos)
		{
			res.setBody(raw);
			res.setContentType("text/html");
			return true;
		}
		headerEnd = headerEndLf;
	}

	std::string headerSection = raw.substr(0, headerEnd);
	std::string bodySection = raw.substr(headerEnd + 2);

	if (!bodySection.empty() && bodySection[0] == '\n')
		bodySection = bodySection.substr(1);

	std::istringstream headerStream(headerSection);
	std::string line;

	int statusCode = 200;
	std::string statusReason = "OK";
	bool hasStatus = false;

	while (std::getline(headerStream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		if (line.empty())
			break;

		std::size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		if (!value.empty() && value[0] == ' ')
			value.erase(0, 1);

		if (key == "Status")
		{
			std::istringstream statusSs(value);
			statusSs >> statusCode;
			if (statusSs)
			{
				std::getline(statusSs, statusReason);
				if (!statusReason.empty() && statusReason[0] == ' ')
					statusReason.erase(0, 1);
			}
			hasStatus = true;
		}
		else if (key == "Location")
		{
			res.setStatus(302, "Found");
			res.setHeader("Location", value);
			res.setBody("");
			res.setContentLength(0);
			return true;
		}
		else if (key == "Content-Type")
		{
			res.setContentType(value);
		}
		else
		{
			res.setHeader(key, value);
		}
	}

	if (hasStatus)
		res.setStatus(statusCode, statusReason);
	else
		res.setStatus(200, "OK");

	res.setBody(bodySection);
	res.setContentLength(bodySection.size());

	return true;
}

bool CGIHandler::execute(HttpResponse &res)
{
	std::string scriptPath = resolveScriptPath();
	std::string interpreter = getInterpreter();

	if (interpreter.empty() || !Utils::fileExists(scriptPath))
	{
		res.setStatus(404, "Not Found");
		std::string body = "<html><body><h1>404 Not Found</h1><p>CGI script not found</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return false;
	}

	int stdinPipe[2];
	int stdoutPipe[2];

	if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0)
	{
		res.setStatus(500, "Internal Server Error");
		std::string body = "<html><body><h1>500 Internal Server Error</h1><p>pipe() failed</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return false;
	}

	char **env = buildEnv();
	char *argv[] = { const_cast<char*>(interpreter.c_str()), const_cast<char*>(scriptPath.c_str()), NULL };

	pid_t pid = fork();
	if (pid < 0)
	{
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
		return false;
	}

	if (pid == 0)
	{
		close(stdinPipe[1]);
		close(stdoutPipe[0]);

		if (dup2(stdinPipe[0], STDIN_FILENO) < 0)
			std::exit(1);
		if (dup2(stdoutPipe[1], STDOUT_FILENO) < 0)
			std::exit(1);

		close(stdinPipe[0]);
		close(stdoutPipe[1]);

		if (interpreter == scriptPath)
			execve(scriptPath.c_str(), argv, env);
		else
			execve(interpreter.c_str(), argv, env);

		std::exit(1);
	}

	close(stdinPipe[0]);
	close(stdoutPipe[1]);

	if (!_req._body.empty())
	{
		std::size_t totalWritten = 0;
		while (totalWritten < _req._body.size())
		{
			ssize_t written = write(stdinPipe[1], _req._body.c_str() + totalWritten,
									_req._body.size() - totalWritten);
			if (written < 0)
				break;
			totalWritten += static_cast<std::size_t>(written);
		}
	}
	close(stdinPipe[1]);

	std::string cgiOutput;
	char buf[4096];
	int timeoutMs = 5000;
	bool timedOut = false;

	while (true)
	{
		struct pollfd pfd;
		std::memset(&pfd, 0, sizeof(pfd));
		pfd.fd = stdoutPipe[0];
		pfd.events = POLLIN;

		int pollRet = ::poll(&pfd, 1, timeoutMs);

		if (pollRet < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}

		if (pollRet == 0)
		{
			timedOut = true;
			kill(pid, SIGTERM);
			break;
		}

		if (pfd.revents & POLLIN)
		{
			ssize_t n = read(stdoutPipe[0], buf, sizeof(buf));
			if (n > 0)
				cgiOutput.append(buf, static_cast<std::size_t>(n));
			else if (n == 0)
				break;
		}
		else
		{
			break;
		}
	}

	close(stdoutPipe[0]);

	int status;
	waitpid(pid, &status, 0);
	freeEnv(env);

	if (timedOut)
	{
		res.setStatus(504, "Gateway Timeout");
		std::string body = "<html><body><h1>504 Gateway Timeout</h1><p>CGI script timed out</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return false;
	}

	if (WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0))
	{
		res.setStatus(500, "Internal Server Error");
		std::string body = "<html><body><h1>500 Internal Server Error</h1><p>CGI script execution failed</p></body></html>";
		res.setBody(body);
		res.setContentType("text/html");
		res.setContentLength(body.size());
		return false;
	}

	return parseOutput(cgiOutput, res);
}

bool isCgiExtension(const std::string &path, const LocationConfig &loc)
{
	if (loc.cgiExt.empty())
		return false;

	std::string cleanPath = path;
	std::size_t qmark = cleanPath.find('?');
	if (qmark != std::string::npos)
		cleanPath = cleanPath.substr(0, qmark);

	std::size_t dot = cleanPath.rfind('.');
	if (dot == std::string::npos)
		return false;

	std::string ext = cleanPath.substr(dot);
	for (std::size_t i = 0; i < loc.cgiExt.size(); ++i)
	{
		if (loc.cgiExt[i] == ext)
			return true;
	}
	return false;
}
