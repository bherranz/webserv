#include "Utils.hpp"

#include <sys/stat.h>
#include <ctime>
#include <fstream>
#include <sstream>
#include <cstdlib>

Utils::Utils() {}
Utils::~Utils() {}
Utils::Utils(const Utils &other) { (void)other; }
Utils &Utils::operator=(const Utils &other) { (void)other; return *this; }

std::string Utils::readFile(const std::string &path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
		return "";
	std::ostringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

bool Utils::fileExists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

bool Utils::isDirectory(const std::string &path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

std::string Utils::getMimeType(const std::string &ext)
{
	if (ext == ".html" || ext == ".htm")
		return "text/html";
	if (ext == ".css")
		return "text/css";
	if (ext == ".js")
		return "application/javascript";
	if (ext == ".json")
		return "application/json";
	if (ext == ".png")
		return "image/png";
	if (ext == ".jpg" || ext == ".jpeg")
		return "image/jpeg";
	if (ext == ".gif")
		return "image/gif";
	if (ext == ".svg")
		return "image/svg+xml";
	if (ext == ".ico")
		return "image/x-icon";
	if (ext == ".pdf")
		return "application/pdf";
	if (ext == ".zip")
		return "application/zip";
	if (ext == ".txt")
		return "text/plain";
	if (ext == ".xml")
		return "application/xml";
	if (ext == ".mp4")
		return "video/mp4";
	if (ext == ".mp3")
		return "audio/mpeg";
	if (ext == ".woff")
		return "font/woff";
	if (ext == ".woff2")
		return "font/woff2";
	if (ext == ".ttf")
		return "font/ttf";
	return "application/octet-stream";
}

std::string Utils::urlDecode(const std::string &url)
{
	std::string result;
	for (std::size_t i = 0; i < url.size(); ++i)
	{
		if (url[i] == '%' && i + 2 < url.size())
		{
			std::string hex = url.substr(i + 1, 2);
			char *end = NULL;
			long c = std::strtol(hex.c_str(), &end, 16);
			if (end != NULL && *end == '\0')
			{
				result += static_cast<char>(c);
				i += 2;
				continue;
			}
		}
		else if (url[i] == '+')
		{
			result += ' ';
			continue;
		}
		result += url[i];
	}
	return result;
}

std::string Utils::extractQueryString(const std::string &path)
{
	std::size_t pos = path.find('?');
	if (pos == std::string::npos)
		return "";
	return path.substr(pos + 1);
}

std::string Utils::stripQueryString(const std::string &path)
{
	std::size_t pos = path.find('?');
	if (pos == std::string::npos)
		return path;
	return path.substr(0, pos);
}

std::string Utils::formatDate()
{
	char buf[128];
	std::time_t now = std::time(NULL);
	std::tm *gmt = std::gmtime(&now);
	if (gmt == NULL)
		return "";
	std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
	return std::string(buf);
}

std::string Utils::joinPath(const std::string &dir, const std::string &path)
{
	if (dir.empty())
		return path;
	if (path.empty())
		return dir;

	bool dirHasSlash = (dir[dir.size() - 1] == '/');
	bool pathHasSlash = (path[0] == '/');

	if (dirHasSlash && pathHasSlash)
		return dir + path.substr(1);
	if (!dirHasSlash && !pathHasSlash)
		return dir + "/" + path;
	return dir + path;
}

std::string Utils::normalizePath(const std::string &path)
{
	std::vector<std::string> segments;
	std::istringstream ss(path);
	std::string segment;

	while (std::getline(ss, segment, '/'))
	{
		if (segment.empty() || segment == ".")
			continue;
		if (segment == "..")
		{
			if (!segments.empty())
				segments.pop_back();
		}
		else
		{
			segments.push_back(segment);
		}
	}

	std::string result;
	for (std::size_t i = 0; i < segments.size(); ++i)
		result += "/" + segments[i];

	if (result.empty())
		result = "/";
	if (!path.empty() && path[path.size() - 1] == '/' && result != "/")
		result += "/";

	return result;
}
