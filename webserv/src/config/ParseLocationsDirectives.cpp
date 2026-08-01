#include "ConfigParser.hpp"

void ConfigParser::parseAllowMethods(const std::vector<std::string>& tokens, LocationConfig& location)
{
	validateSemicolon(tokens);
	if (tokens.size() < 3)
		throw std::runtime_error("config error: missing methods in 'allow_methods'");

	for (std::size_t i = 1; i < tokens.size() - 1; ++i)
	{
		std::string method = tokens[i]; //again we separate the metrhos in our metehod vector
		if (method != "GET" && method != "POST" && method != "DELETE")//for now we only have the 3 principal methods as requested in the subject
			throw std::runtime_error("config error: invalid method '" + method + "' in 'allow_methods'");
		location.allowMethods.push_back(method); //add it to our vector of methods
	}
	location.hasAllowMethods = true;
}

/*void ConfigParser::parseAutoindex(const std::vector<std::string>& tokens, LocationConfig& location)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid arguments in 'autoindex'");

	if (tokens[1] == "on")
		location.autoindex = true;
	else if (tokens[1] == "off")
		location.autoindex = false;
	else
		throw std::runtime_error("config error: autoindex must be 'on' or 'off'");

	location.hasAutoindex = true;
}*/

void ConfigParser::parseUploadStore(const std::vector<std::string>& tokens, LocationConfig& location)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid arguments in 'upload_store'");

	location.uploadStore = tokens[1];
	location.hasUploadStore = true;
}

void ConfigParser::parseCgiPath(const std::vector<std::string>& tokens, LocationConfig& location)
{
	validateSemicolon(tokens);
	if (tokens.size() < 3)
		throw std::runtime_error("config error: missing arguments in 'cgi_path'");

	for (std::size_t i = 1; i < tokens.size() - 1; ++i)
		location.cgiPath.push_back(tokens[i]);  //do we check here wether if the path is allowed/exists?
}

void ConfigParser::parseCgiExt(const std::vector<std::string>& tokens, LocationConfig& location)
{
	validateSemicolon(tokens);
	if (tokens.size() < 3)
		throw std::runtime_error("config error: missing arguments in 'cgi_ext'");

	for (std::size_t i = 1; i < tokens.size() - 1; ++i)
		location.cgiExt.push_back(tokens[i]);
}

void ConfigParser::parseReturn(const std::vector<std::string>& tokens, LocationConfig& location)
{
	validateSemicolon(tokens);

	// NGINX has 2 types of redirections
	// Format 1: return 301 /redirect_path;
	if (tokens.size() == 4)
	{
		location.redirectCode = std::atoi(tokens[1].c_str());
		location.redirectTarget = tokens[2];
		location.hasRedirect = true;
	} 
	// Format 2: return /redirect_path; (BEWARE BY DEFECT WE PUT AS 302 o 301)
	else if (tokens.size() == 3)
	{
		location.redirectCode = 301;
		location.redirectTarget = tokens[1];
		location.hasRedirect = true;
	}
	else
		throw std::runtime_error("config error: invalid arguments in 'return'");
}