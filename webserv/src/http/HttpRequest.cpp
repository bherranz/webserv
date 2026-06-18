#include "HttpRequest.hpp"

#include <cstdlib>

HttpRequest::HttpRequest()
	: _headersDone(false), _contentLength(0), _chunked(false) {}

bool HttpRequest::headersComplete() const
{
	return _headersDone;
}

bool HttpRequest::bodyComplete()
{
	if (!_headersDone)
		return false;
	if (_chunked)
	{
		if (_body.find("0\r\n\r\n") != std::string::npos)
		{
			parseChunkedBody();
			_chunked = false;
			_contentLength = _body.size();
			return true;
		}
		return false;
	}
	if (_contentLength == 0)
		return true;
	return _body.size() >= _contentLength;
}

void HttpRequest::appendBodyData(const std::string &data)
{
	_body.append(data);
}

bool HttpRequest::parse(const std::string &raw)
{
	std::istringstream stream(raw);
	std::string line;

	if (_method.empty())
	{
		if (!std::getline(stream, line))
			return false;
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		parseRequestLine(line);
	}

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
		{
			_headersDone = true;
			break;
		}
		if (_headers.find(line.substr(0, line.find(':'))) == _headers.end())
			parseHeaderLine(line);
	}

	if (_headersDone)
	{
		std::map<std::string, std::string>::iterator cl = _headers.find("Content-Length");
		if (cl != _headers.end())
		{
			char *end = NULL;
			long len = std::strtol(cl->second.c_str(), &end, 10);
			if (end != NULL && *end == '\0' && len >= 0)
				_contentLength = static_cast<std::size_t>(len);
		}

		std::map<std::string, std::string>::iterator te = _headers.find("Transfer-Encoding");
		if (te != _headers.end() && te->second.find("chunked") != std::string::npos)
			_chunked = true;
	}

	return true;
}

bool HttpRequest::parseChunkedBody()
{
	std::string decoded;
	std::size_t i = 0;

	while (i < _body.size())
	{
		std::size_t endLine = _body.find("\r\n", i);
		if (endLine == std::string::npos)
			break;

		std::string hexStr = _body.substr(i, endLine - i);
		char *end = NULL;
		long chunkSize = std::strtol(hexStr.c_str(), &end, 16);
		if (end == NULL || *end != '\0' || chunkSize < 0)
			return false;

		if (chunkSize == 0)
		{
			_body = decoded;
			return true;
		}

		std::size_t chunkStart = endLine + 2;
		if (chunkStart + static_cast<std::size_t>(chunkSize) > _body.size())
			break;

		decoded.append(_body, chunkStart, static_cast<std::size_t>(chunkSize));
		i = chunkStart + static_cast<std::size_t>(chunkSize) + 2;
	}

	_body = decoded;
	return true;
}

void HttpRequest::parseRequestLine(const std::string &line)
{
	std::istringstream headerData(line);

	headerData >> _method;
	headerData >> _path;
	headerData >> _version;
}

void HttpRequest::parseHeaderLine(const std::string &line)
{
	std::size_t separator = line.find(':');

	if (separator == std::string::npos)
		return;

	std::string key = line.substr(0, separator);
	std::string value = line.substr(separator + 1);

	if (!value.empty() && value[0] == ' ')
		value.erase(0, 1);

	_headers[key] = value;
}

void HttpRequest::printRequest() const
{
	std::cout << "===== HTTP REQUEST =====" << std::endl;

	std::cout << "METHOD: " << _method << std::endl;
	std::cout << "PATH: " << _path << std::endl;
	std::cout << "VERSION: " << _version << std::endl;

	std::cout << "\n--- HEADERS ---" << std::endl;

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
		 it != _headers.end(); ++it)
		std::cout << it->first << " => " << it->second << std::endl;

	if (!_body.empty())
	{
		std::cout << "\n--- BODY (" << _body.size() << " bytes) ---" << std::endl;
		std::cout << _body << std::endl;
	}

	std::cout << "========================" << std::endl;
}
