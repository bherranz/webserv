#pragma once

#include <string>
#include <vector>
#include <sstream>

class Utils
{
	private:
		Utils();
		~Utils();
		Utils(const Utils &other);
		Utils &operator=(const Utils &other);

	public:
		template <typename T>
		static std::string toString(const T &value)
		{
			std::ostringstream oss;
			oss << value;
			return oss.str();
		}

		static std::string readFile(const std::string &path);
		static bool fileExists(const std::string &path);
		static bool isDirectory(const std::string &path);
		static std::string getMimeType(const std::string &ext);
		static std::string urlDecode(const std::string &url);
		static std::string extractQueryString(const std::string &path);
		static std::string stripQueryString(const std::string &path);
		static std::string formatDate();
		static std::string joinPath(const std::string &dir, const std::string &path);
};

