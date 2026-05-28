#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>

#include "Config.hpp"

class ConfigParser {
public:
	static Config parseFile(const std::string &path);
};

#endif
