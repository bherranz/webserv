#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

struct ListenConfig
{
	std::string	host;
	int			port;

	ListenConfig();
	ListenConfig(const std::string &listenHost, int listenPort);
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

	ServerConfig();
};

class Config
{
	public:
	Config();
	~Config();
	explicit Config(const std::string &path);

	void	load(const std::string &path);

	std::vector<ServerConfig> &servers();
	const std::vector<ServerConfig> &servers() const;

	private:
	std::vector<ServerConfig> _servers;
};
