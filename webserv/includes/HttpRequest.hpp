#pragma once

#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdlib>

class HttpRequest
{
	public:
	std::string _method;
	std::string _path;
	std::string _version;

	std::map<std::string, std::string> _headers;
	std::string _body;

	HttpRequest();
	bool parse(const std::string &raw);
	void printRequest() const;
	bool headersComplete() const;
	bool bodyComplete();
	void appendBodyData(const std::string &data);
	bool isChunked() const;
	void clear();

	private:
	bool parseRequestLine(const std::string &line);
	void parseHeaderLine(const std::string &line);
	bool parseChunkedBody();

	bool _headersDone;
	std::size_t _contentLength;
	bool _chunked;
};
