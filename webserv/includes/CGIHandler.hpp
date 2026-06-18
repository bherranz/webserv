#pragma once

#include <string>
#include <vector>
#include <map>

#include "Config.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class CGIHandler
{
	public:
	CGIHandler(const HttpRequest &req, const LocationConfig &loc, const ServerConfig &server);
	~CGIHandler();

	bool execute(HttpResponse &res);

	private:
	std::string resolveScriptPath() const;
	std::string getInterpreter() const;
	char **buildEnv() const;
	bool parseOutput(const std::string &raw, HttpResponse &res);

	static std::string envEscape(const std::string &str);

	const HttpRequest &_req;
	const LocationConfig &_loc;
	const ServerConfig &_server;
};

bool isCgiExtension(const std::string &path, const LocationConfig &loc);
