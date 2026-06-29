/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:55:40 by miparis           #+#    #+#             */
/*   Updated: 2026/06/29 12:39:51 by miparis          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Config.hpp"
#include "ConfigParser.hpp"

Config::Config()
{
}

ListenConfig:: ListenConfig()
{

}
ListenConfig:: ListenConfig(const std::string &listenHost, int listenPort): host(listenHost), port(listenPort)
{
	
}

std::vector<ServerConfig> &Config::servers()
{
    return _servers;
}

const std::vector<ServerConfig> &Config::servers() const
{
    return _servers;
}

ConfigParser::ConfigParser()
{
	initDirectiveParsers();
}

ServerConfig::ServerConfig() : 
    clientMaxBodySize(0), clientTimeout(60), keepaliveTimeout(10),
    hasHost(false), hasRoot(false), hasIndex(false), 
    hasClientMaxBodySize(false), hasClientTimeout(false), hasKeepaliveTimeout(false) 
{
}

LocationConfig::LocationConfig() : 
    autoindex(false), redirectCode(0), clientMaxBodySize(0),
    hasRoot(false), hasUploadStore(false), hasIndex(false), 
    hasAllowMethods(false), hasRedirect(false), 
    hasClientMaxBodySize(false), hasAutoindex(false) 
{
}