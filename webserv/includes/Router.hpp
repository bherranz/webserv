#pragma once

#include "Config.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "CGIHandler.hpp"

class Router
{
	public:
	Router();
	~Router();

	CgiFds route(const HttpRequest &req, HttpResponse &res, const ServerConfig &server);

	private:
	const LocationConfig *matchLocation(const std::string &path, const ServerConfig &server) const;
	bool isMethodAllowed(const std::string &method, const LocationConfig &loc) const;
	std::string resolvePath(const std::string &uriPath, const LocationConfig &loc, const ServerConfig &server) const;
	std::string resolveIndex(const std::string &dirPath, const LocationConfig &loc, const ServerConfig &server) const;

	void handleGet(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server);
	void handlePost(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server);
	void handleDelete(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server);
	void handleRedirect(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc);
	CgiFds handleCgi(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server);
	void handleAutoindex(const std::string &dirPath, HttpResponse &res);

	void buildErrorResponse(HttpResponse &res, int code, const ServerConfig &server);

	bool parseMultipart(const std::string &body, const std::string &boundary,
		std::string &outFilename, std::string &outContent);
};
