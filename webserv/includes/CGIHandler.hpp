#pragma once

#include <string>
#include <vector>
#include <map>
#include <sys/types.h>

#include "Config.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

// struct to hold the file descriptors and process ID for a CGI process
struct CgiFds {
	pid_t pid; // process ID of the CGI process
	int readFd; // fd for reading from the CGI process
	int writeFd; // fd for writing to the CGI process
};

class CGIHandler {
	public:
		CGIHandler(const HttpRequest &req, const LocationConfig &loc, const ServerConfig &server);
		~CGIHandler();

		// starts the child process and returns the fds
		CgiFds start(HttpResponse &res);
		// when the server reads the output, it calls this function to finalize the response
		static bool finalize(const std::string &rawOutput, HttpResponse &res);
		static std::string getInterpreter(const std::string &path, const LocationConfig &loc);

	private:
		std::string resolveScriptPath() const;
		std::string getInterpreter() const;
		char **buildEnv() const;
		void freeEnv(char **env) const;
		void executeChild(int stdinPipe[2], int stdoutPipe[2], const std::string &scriptPath, 
                      const std::string &interpreter, char **env) const;
		static void parseCgiHeaders(const std::string &headerSection, HttpResponse &res);

		// original references
		const HttpRequest &_req;
		const LocationConfig &_loc;
		const ServerConfig &_server;
};

bool isCgiExtension(const std::string &path, const LocationConfig &loc);
