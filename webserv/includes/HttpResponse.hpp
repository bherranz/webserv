#pragma once

#include <string>
#include <sstream>
#include <map>
#include "Utils.hpp"

class HttpResponse
{
	public:
	HttpResponse();
	~HttpResponse();

	void setStatus(int code, const std::string &reason);
	void setHeader(const std::string &key, const std::string &value);
	void setBody(const std::string &body);
	void setContentType(const std::string &mime);
	void setContentLength(std::size_t len);

	std::string toString() const;

	int statusCode() const;

	void clear();
	void setError(int code, const std::string &detail = "");

	static std::string reasonPhrase(int code);

	private:
	int _statusCode;
	std::string _statusReason;
	std::map<std::string, std::string> _headers;
	std::string _body;
};
