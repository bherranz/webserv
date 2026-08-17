#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

const std::string GREEN  = "\033[0;32m";
const std::string YELLOW = "\033[0;33m";
const std::string RED    = "\033[0;31m";
const std::string BLUE   = "\033[1;34m";
const std::string PURPLE = "\033[0;35m";
const std::string BOLD   = "\033[1m";
const std::string NC     = "\033[0m";

struct ListenConfig
{
	std::string	host;
	int			port;

	ListenConfig();
	ListenConfig(const std::string &listenHost, int listenPort);//check, tenemos los valores de port y host separados gracias al parseo del config ver si lo reemplazamos fuera del const
};

struct LocationConfig
{
	std::string				path;
	std::string				root;
	std::string				uploadStore;
	std::vector<std::string>	index;
	bool					autoindex;
	std::vector<std::string>	allowMethods;
	std::string				redirectTarget;
	int						redirectCode;
	std::size_t				clientMaxBodySize;
	std::vector<std::string>	cgiPath;
	std::vector<std::string>	cgiExt;

	bool	hasRoot;
	bool	hasUploadStore;
	bool	hasIndex;
	bool	hasAllowMethods;
	bool	hasRedirect;
	bool	hasClientMaxBodySize;
	bool	hasAutoindex;

	LocationConfig();
};

struct ServerConfig
{
	std::string						host;
	std::vector<ListenConfig>		listens;
	std::vector<std::string>		serverNames;
	std::string						root;
	std::vector<std::string>		index;
	std::map<int, std::string>		errorPages;
	std::size_t						clientMaxBodySize;
	std::vector<LocationConfig>		locations;

	int		clientTimeout;
	int		keepaliveTimeout;

	bool	hasHost;
	bool	hasRoot;
	bool	hasIndex;
	bool	hasClientMaxBodySize;
	bool	hasClientTimeout;
	bool	hasKeepaliveTimeout;
	bool	autoindex;
	bool	hasAutoindex;

	ServerConfig();
};

class Config
{
	public:
	Config();
	explicit Config(const std::string &path);

	void	load(const std::string &path);

	std::vector<ServerConfig> &servers();
	const std::vector<ServerConfig> &servers() const;
	void print() const;
	void validate() const;

	private:
	std::vector<ServerConfig> _servers;
};
