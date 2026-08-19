#include "HttpRequest.hpp"

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
		try
		{
			if (parseChunkedBody())
			{
				_chunked = false;
				_contentLength = _body.size();
				return true;
			}
			return false;
		}
		catch ( const std::exception& e)
		{
			throw std::runtime_error("Header error: Malformed chunked body");
		}
	}

	if (_contentLength == 0)
		return true;
	return _body.size() >= _contentLength;
}

void HttpRequest::appendBodyData(const std::string &data)
{
	_body.append(data);
}

void HttpRequest::clear()
{
	_method.clear();
	_path.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_headersDone = false;
	_contentLength = 0;
	_chunked = false;
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
		if (!parseRequestLine(line))
			return false;
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
		parseHeaderLine(line);
	}

	if (_headersDone)
	{
		if (_version == "HTTP/1.1" && _headers.find("host") == _headers.end())
			return false;

		std::map<std::string, std::string>::iterator te = _headers.find("transfer-encoding");
		if (te != _headers.end() && te->second.find("chunked") != std::string::npos)
		{
			_chunked = true;
		}
		else
		{
			std::map<std::string, std::string>::iterator cl = _headers.find("content-length");
			if (cl != _headers.end())
			{
				char *end = NULL;
				long len = std::strtol(cl->second.c_str(), &end, 10);
				if (end != NULL && *end == '\0' && len >= 0)
					_contentLength = static_cast<std::size_t>(len);
				else
					return false;
			}
		}
	}

	return true;
}

bool HttpRequest::isChunked() const
{
	return _chunked;
}

bool HttpRequest::parseChunkedBody()
{
	std::string decoded;
	std::size_t i = 0;

	while (i < _body.size())
	{
		std::size_t endLine = _body.find("\r\n", i);
		if (endLine == std::string::npos)
			return false;

		std::string hexStr = _body.substr(i, endLine - i);

		std::size_t semi = hexStr.find(';');
		if (semi != std::string::npos)
			hexStr = hexStr.substr(0, semi);

		if (hexStr.empty() || hexStr.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
			throw std::runtime_error("Invalid chunk size format");

		char *end = NULL;
		long chunkSize = std::strtol(hexStr.c_str(), &end, 16);
		if (end == NULL || *end != '\0' || chunkSize < 0)
			throw std::runtime_error("Malformed chunk size");

		if (chunkSize == 0)
		{
			std::size_t trailerEnd = _body.find("\r\n\r\n", endLine);
			if (trailerEnd != std::string::npos)
			{
				_body = decoded;
				return true;
			}
			return false;
		}

		std::size_t chunkDataStart = endLine + 2;
		std::size_t nextChunkStart = chunkDataStart + static_cast<std::size_t>(chunkSize) + 2; // +2 for trailing CRLF

		if (nextChunkStart > _body.size())
			return false; // Incomplete chunk

		// Check for \r\n at end of chunk data
		if (_body.compare(chunkDataStart + static_cast<std::size_t>(chunkSize), 2, "\r\n") != 0)
			throw std::runtime_error("Missing CRLF after chunk data");

		decoded.append(_body, chunkDataStart, static_cast<std::size_t>(chunkSize));
		i = nextChunkStart;
	}

	return false;
}

bool HttpRequest::parseRequestLine(const std::string &line)
{
	std::istringstream headerData(line);

	if (!(headerData >> _method >> _path >> _version))
		return false;

	std::string extra;
	if (headerData >> extra)
		return false;

	if (_path.empty() || _path[0] != '/')
		return false;

	if (_version != "HTTP/1.1" && _version != "HTTP/1.0")
		return false;

	return true;
}

void HttpRequest::parseHeaderLine(const std::string &line)
{
	std::size_t separator = line.find(':');

	if (separator == std::string::npos)
		return;

	std::string key = line.substr(0, separator);
	std::string value = line.substr(separator + 1);

	while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
		value.erase(0, 1);
	while (!value.empty() && (value[value.size() - 1] == ' ' || value[value.size() - 1] == '\t'))
		value.erase(value.size() - 1);
	
	for (std::size_t i = 0; i < key.length(); ++i)
		key[i] = std::tolower(static_cast<unsigned char>(key[i]));

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
