#include "HttpResponse.hpp"

HttpResponse::HttpResponse()
	: _statusCode(200), _statusReason("OK") {}

HttpResponse::~HttpResponse() {}

void HttpResponse::setStatus(int code, const std::string &reason)
{
	_statusCode = code;
	_statusReason = reason;
}

void HttpResponse::setHeader(const std::string &key, const std::string &value)
{
	_headers[key] = value;
}

void HttpResponse::setBody(const std::string &body)
{
	_body = body;
}

void HttpResponse::setContentType(const std::string &mime)
{
	_headers["Content-Type"] = mime;
}

void HttpResponse::setContentLength(std::size_t len)
{
	_headers["Content-Length"] = Utils::toString(len);
}

int HttpResponse::statusCode() const
{
	return _statusCode;
}

void HttpResponse::clear()
{
	_statusCode = 200;
	_statusReason = "OK";
	_headers.clear();
	_body.clear();
}

void HttpResponse::setError(int code, const std::string &detail)
{
	_statusCode = code;
	_statusReason = reasonPhrase(code);

	std::string codeStr = Utils::toString(code);
	std::string body = "<html><head><title>" + codeStr + " " + _statusReason + "</title></head><body>";
	body += "<h1>" + codeStr + " " + _statusReason + "</h1>";
	if (!detail.empty())
		body += "<p>" + detail + "</p>";
	body += "</body></html>";

	setBody(body);
	setContentType("text/html");
	setContentLength(body.size());
}

std::string HttpResponse::toString() const
{
	std::ostringstream response;

	response << "HTTP/1.1 " << _statusCode << " " << _statusReason << "\r\n";

	if (_headers.find("Server") == _headers.end())
		response << "Server: webserv/1.0\r\n";
	if (_headers.find("Date") == _headers.end())
		response << "Date: " << Utils::formatDate() << "\r\n";

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
		 it != _headers.end(); ++it)
		response << it->first << ": " << it->second << "\r\n";

	response << "\r\n";
	response << _body;

	return response.str();
}

std::string HttpResponse::reasonPhrase(int code)
{
	switch (code)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 307: return "Temporary Redirect";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 413: return "Content Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		default: return "Unknown";
	}
}
