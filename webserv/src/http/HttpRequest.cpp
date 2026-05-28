/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:54:54 by miparis           #+#    #+#             */
/*   Updated: 2026/05/28 15:35:03 by miparis          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"

bool HttpRequest::parse(const std::string &_raw)
{
	std::istringstream _stream(_raw); //treating all of the line like a "file"
	std::string _line;

	// REQUEST LINE
	if (!std::getline(_stream, _line))
		return (false);

	if (!_line.empty() && _line[_line.size() - 1] == '\r')
		_line.erase(_line.size() - 1);

	parseRequestLine(_line);

	// HEADERS
	while (std::getline(_stream, _line))
	{
		if (!_line.empty() && _line[_line.size() - 1] == '\r')
			_line.erase(_line.size() - 1);//delete \r for corret saving
		if (_line.empty())
			break;
		parseHeaderLine(_line);
	}
	return (true);
}

void  HttpRequest::parseRequestLine(const std::string &line)
{
	std::istringstream _headerData(line);

		_headerData >> _method;
		_headerData >> _path;
		_headerData >> _version;
}

void  HttpRequest::parseHeaderLine(const std::string &line)
{
std::size_t _separator = line.find(':');

	if (_separator == std::string::npos)
		return;

	std::string _key = line.substr(0, _separator);

	std::string _value = line.substr(_separator + 1);

	// get rid of the space empty at the start
	if (!_value.empty() && _value[0] == ' ')
		_value.erase(0, 1);

	_headers[_key] = _value; // we save the _key & value for each line
}

void HttpRequest::printRequest() const
{
	std::cout << "===== HTTP REQUEST =====" << std::endl;

	std::cout << "METHOD: " << _method << std::endl;
	std::cout << "PATH: " << _path << std::endl;
	std::cout << "VERSION: " << _version << std::endl;

	std::cout << "\n--- HEADERS ---" << std::endl;

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
		 it != _headers.end();
		 ++it)
	{
		std::cout << it->first
				  << " => "
				  << it->second
				  << std::endl;
	}

	if (!_body.empty())
	{
		std::cout << "\n--- BODY ---" << std::endl;
		std::cout << _body << std::endl;
	}

	std::cout << "========================" << std::endl;
}