#include "ConfigParser.hpp"

void ConfigParser::parseListen(const std::vector<std::string>& tokens, ServerConfig& server)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid number of arguments in 'listen'");

	std::string listenToken = tokens[1];
	std::string host = "0.0.0.0";// Default
	int port = 8080;// Default port

	std::size_t colonPos = listenToken.find(':');//we save the ':' location to further separate te host/port pair

	if (colonPos != std::string::npos)
	{
		// HOST:PORT (ej. 127.0.0.1:8080)
		host = listenToken.substr(0, colonPos);
		std::string portStr = listenToken.substr(colonPos + 1);
		port = std::atoi(portStr.c_str());
	}
	else
	{
		// Only HOST o or only PORT
		// If we find a '.', we assume its a IP. If not, a port.
		if (listenToken.find('.') != std::string::npos || listenToken == "localhost")
		{
			host = listenToken;
			port = 8080; // !!!!!! If only IP, NGINX uses 80 commonly, but we put 8080 for security!!!!!!!!!!!!!!!!!!!!!!!
		}
		else
			port = std::atoi(listenToken.c_str());
	}
	if (port <= 0 || port > 65535)
		throw std::runtime_error("config error: invalid port number");

	server.listens.push_back(ListenConfig(host, port));
}

void ConfigParser::parseServerName(const std::vector<std::string>& tokens, ServerConfig& server)
{
	validateSemicolon(tokens);
	if (tokens.size() < 3)
		throw std::runtime_error("config error: missing arguments in 'server_name'");

	for (std::size_t i = 1; i < tokens.size() - 1; ++i)
		server.serverNames.push_back(tokens[i]);
}

void ConfigParser::parseErrorPage(const std::vector<std::string>& tokens, ServerConfig& server)
{
	validateSemicolon(tokens);
	// ej: error_page 404 505 402 /404.html; 
	if (tokens.size() < 4)
		throw std::runtime_error("config error: invalid arguments in 'error_page'");

	std::string pagePath = tokens[tokens.size() - 2]; //always the last one before the ; is the path to the file
	for (std::size_t i = 1; i < tokens.size() - 2; ++i)
	{
		int errorCode = std::atoi(tokens[i].c_str());
		if (errorCode < 300 || errorCode > 599)
			throw std::runtime_error("config error: invalid HTTP error code in 'error_page'");
		server.errorPages[errorCode] = pagePath; //we save the differents code with the same path 
	}
}
//->> NGNIX DOEST SAVE THE HOST INDEPENDANTLY, IT ALWAYS GOES IN THE LISTEN BLOCK -- CHECK IF DELETE
void ConfigParser::parseHost(const std::vector<std::string>& tokens, ServerConfig& server)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid number of arguments in 'host'");

	server.host = tokens[1];
	server.hasHost = true;
}
void ConfigParser::parseClientTimeout(const std::vector<std::string>& tokens, ServerConfig& server)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid arguments in 'client_timeout'");

	server.clientTimeout = std::atoi(tokens[1].c_str());
	server.hasClientTimeout = true;
}
void ConfigParser::parseKeepaliveTimeout(const std::vector<std::string>& tokens, ServerConfig& server)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid arguments in 'keepalive_timeout'");

	server.keepaliveTimeout = std::atoi(tokens[1].c_str());
	server.hasKeepaliveTimeout = true;
}

template <typename T>
void ConfigParser::parseAutoindex(const std::vector<std::string>& tokens, T& config)
{
    validateSemicolon(tokens);
    if (tokens.size() != 3)
        throw std::runtime_error("config error: invalid arguments in 'autoindex'");

    if (tokens[1] == "on")
        config.autoindex = true;
    else if (tokens[1] == "off")
        config.autoindex = false;
    else
        throw std::runtime_error("config error: autoindex must be 'on' or 'off'");
    
    config.hasAutoindex = true;
}