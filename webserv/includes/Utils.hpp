#pragma once

#include <string>
#include <vector>

namespace Utils
{
	std::string readFile(const std::string &path);
	bool fileExists(const std::string &path);
	bool isDirectory(const std::string &path);
	std::string getMimeType(const std::string &ext);
	std::string urlDecode(const std::string &url);
	std::string extractQueryString(const std::string &path);
	std::string stripQueryString(const std::string &path);
	std::string formatDate();
}
