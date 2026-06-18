/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jaime <jaime@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:55:40 by miparis           #+#    #+#             */
/*   Updated: 2026/06/18 17:18:12 by jaime            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Config.hpp"
#include "ConfigParser.hpp"

ListenConfig::ListenConfig() : host("0.0.0.0"), port(8080) {}

ListenConfig::ListenConfig(const std::string &listenHost, int listenPort)
	: host(listenHost), port(listenPort) {}

LocationConfig::LocationConfig()
	: path(""), root(""), uploadStore(""), index(), autoindex(false),
	  allowMethods(), redirectTarget(""), redirectCode(301),
	  clientMaxBodySize(0), cgiPath(), cgiExt(),
	  hasRoot(false), hasUploadStore(false), hasIndex(false),
	  hasAllowMethods(false), hasRedirect(false),
	  hasClientMaxBodySize(false), hasAutoindex(false) {}

ServerConfig::ServerConfig()
	: host("0.0.0.0"), root(""), clientMaxBodySize(0),
	  clientTimeout(60), keepaliveTimeout(10),
	  hasHost(false), hasRoot(false), hasIndex(false),
	  hasClientMaxBodySize(false), hasClientTimeout(false), hasKeepaliveTimeout(false) {}

Config::Config() {}
Config::~Config() {}

Config::Config(const std::string &path) { load(path); }

void Config::load(const std::string &path) {
	Config parsed = ConfigParser::parseFile(path);
	_servers = parsed.servers();
}

std::vector<ServerConfig> &Config::servers() { return _servers; }

const std::vector<ServerConfig> &Config::servers() const { return _servers; }