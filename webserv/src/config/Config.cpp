/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jaime <jaime@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:55:40 by miparis           #+#    #+#             */
/*   Updated: 2026/05/28 13:05:00 by jaime            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Config.hpp"
#include "ConfigParser.hpp"

ListenConfig::ListenConfig() : host("0.0.0.0"), port(8080) {}

ListenConfig::ListenConfig(const std::string &listenHost, int listenPort)
	: host(listenHost), port(listenPort) {}

LocationConfig::LocationConfig()
	: path(""), root(""), redirectTarget(""), redirectCode(301), clientMaxBodySize(0),
	  autoindex(false), hasRoot(false), hasIndex(false), hasAllowMethods(false),
	  hasRedirect(false), hasClientMaxBodySize(false), hasAutoindex(false) {}

ServerConfig::ServerConfig()
	: host("0.0.0.0"), root(""), clientMaxBodySize(0), hasHost(false), hasRoot(false),
	  hasIndex(false), hasClientMaxBodySize(false) {}

Config::Config() {}

Config::Config(const std::string &path) { load(path); }

void Config::load(const std::string &path) {
	Config parsed = ConfigParser::parseFile(path);
	_servers = parsed.servers();
}

std::vector<ServerConfig> &Config::servers() { return _servers; }

const std::vector<ServerConfig> &Config::servers() const { return _servers; }/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:55:40 by miparis           #+#    #+#             */
/*   Updated: 2026/03/20 15:55:51 by miparis          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Config.hpp"