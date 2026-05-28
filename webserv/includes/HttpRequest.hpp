/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/28 13:17:56 by miparis           #+#    #+#             */
/*   Updated: 2026/05/28 15:36:46 by miparis          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <map>
#include <string>
#include <sstream>
#include <iostream>

class HttpRequest
{
	public:
	//basic data of a request line 1º line
	std::string _method;
	std::string _path;
	std::string _version;

	std::map<std::string, std::string> _headers;// We have key and value for each one
	std::string _body;
	bool parse(const std::string &raw);
	void printRequest() const;

	private:
	void parseRequestLine(const std::string &line);
	void parseHeaderLine(const std::string &line);
};