/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:54:10 by miparis           #+#    #+#             */
/*   Updated: 2026/05/28 15:36:14 by miparis          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "HttpRequest.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <map>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace {
	bool shouldUsePassive(const std::string &host) {
		return host.empty() || host == "0.0.0.0" || host == "::";
	}

	bool isWildcardHost(const std::string &host) {
		return host.empty() || host == "0.0.0.0";
	}
}

Server::Server(const Config &config) : _config(config) { initListenSockets(); }

Server::~Server() {
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		it->second.closeFd();
	for (std::size_t i = 0; i < _listenFds.size(); ++i)
		::close(_listenFds[i]);
}

void Server::setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl(F_SETFL) failed");
}

void Server::initListenSockets() {
	std::map<int, ListenConfig> chosenByPort;
	for (std::size_t serverIndex = 0; serverIndex < _config.servers().size(); ++serverIndex) {
		const ServerConfig &server = _config.servers()[serverIndex];
		for (std::size_t listenIndex = 0; listenIndex < server.listens.size(); ++listenIndex) {
			const ListenConfig &listenConfig = server.listens[listenIndex];
			std::map<int, ListenConfig>::iterator chosen = chosenByPort.find(listenConfig.port);
			if (chosen == chosenByPort.end() || (isWildcardHost(listenConfig.host) && !isWildcardHost(chosen->second.host)))
				chosenByPort[listenConfig.port] = listenConfig;
		}
	}
	for (std::map<int, ListenConfig>::const_iterator it = chosenByPort.begin(); it != chosenByPort.end(); ++it)
		openListenSocket(it->second, 0);
	if (_listenFds.empty())
		throw std::runtime_error("configuration does not define any listen endpoints");
}

void Server::openListenSocket(const ListenConfig &listenConfig, std::size_t serverIndex) {
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = shouldUsePassive(listenConfig.host) ? AI_PASSIVE : 0;

	std::ostringstream portText;
	portText << listenConfig.port;
	struct addrinfo *result = NULL;
	const char *host = shouldUsePassive(listenConfig.host) ? NULL : listenConfig.host.c_str();
	int err = ::getaddrinfo(host, portText.str().c_str(), &hints, &result);
	if (err != 0) {
		std::string message = "getaddrinfo() failed: ";
		message += ::gai_strerror(err);
		throw std::runtime_error(message);
	}

	int listenFd = -1;
	for (struct addrinfo *current = result; current != NULL; current = current->ai_next) {
		listenFd = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
		if (listenFd < 0)
			continue;

		int yes = 1;
		if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
			::close(listenFd);
			listenFd = -1;
			continue;
		}

		try {
			setNonBlocking(listenFd);
		} catch (...) {
			::close(listenFd);
			listenFd = -1;
			continue;
		}

		if (::bind(listenFd, current->ai_addr, current->ai_addrlen) < 0) {
			::close(listenFd);
			listenFd = -1;
			continue;
		}

		if (::listen(listenFd, SOMAXCONN) < 0) {
			::close(listenFd);
			listenFd = -1;
			continue;
		}
		break;
	}
	::freeaddrinfo(result);

	if (listenFd < 0) {
		std::ostringstream message;
		message << "unable to open listen socket on " << listenConfig.host << ':' << listenConfig.port;
		throw std::runtime_error(message.str());
	}

	_listenFds.push_back(listenFd);
	_listenOwners[listenFd] = serverIndex;
	std::cout << "Listening on http://" << listenConfig.host << ':' << listenConfig.port << std::endl;
}

void Server::rebuildPollFds() {
	_pollFds.clear();

	for (std::size_t i = 0; i < _listenFds.size(); ++i) {
		struct pollfd listenPollFd;
		std::memset(&listenPollFd, 0, sizeof(listenPollFd));
		listenPollFd.fd = _listenFds[i];
		listenPollFd.events = POLLIN;
		_pollFds.push_back(listenPollFd);
	}

	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		struct pollfd c;
		std::memset(&c, 0, sizeof(c));
		c.fd = it->first;
		c.events = POLLIN;
		if (!it->second.outBuffer().empty())
			c.events |= POLLOUT;
		_pollFds.push_back(c);
	}
}

void Server::acceptClients(int listenFd) {
	while (true) {
		int clientFd = ::accept(listenFd, NULL, NULL);
		if (clientFd < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return;
			std::cerr << "accept() failed: " << std::strerror(errno) << std::endl;
			return;
		}
		try {
			setNonBlocking(clientFd);
		} catch (...) {
			::close(clientFd);
			continue;
		}
		_clients.insert(std::make_pair(clientFd, Client(clientFd)));
	}
}

void Server::handleClientRead(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	char buf[4096];
	while (true)
	{
		ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
		if (n > 0)
		{
			it->second.inBuffer().append(buf, n);
			if (it->second.inBuffer().find("\r\n\r\n") != std::string::npos)
			{
				HttpRequest request;
				std::cout << "RAW REQUEST:" << std::endl;
				std::cout << it->second.inBuffer() << std::endl;
				if (request.parse(it->second.inBuffer()))
				{
					std::cout << "METHOD: "
							<< request._method
							<< std::endl;

					std::cout << "PATH: "
							<< request._path
							<< std::endl;

					std::cout << "VERSION: "
							<< request._version
							<< std::endl;
				}
				request.printRequest();
				if (it->second.outBuffer().empty())
				{
					it->second.outBuffer() =
						"HTTP/1.1 200 OK\r\n"
						"Content-Length: 2\r\n"
						"\r\n"
						"OK";
				}

				it->second.inBuffer().clear();
			}
		}
		if (n == 0)
		{
			closeClient(fd);
			return;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		closeClient(fd);
		return;
	}
}

void Server::handleClientWrite(int fd) {
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	std::string &out = it->second.outBuffer();
	while (!out.empty()) {
		ssize_t n = ::send(fd, out.c_str(), out.size(), 0);
		if (n > 0) {
			out.erase(0, n);
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		closeClient(fd);
		return;
	}
	closeClient(fd);
}

void Server::closeClient(int fd) {
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	it->second.closeFd();
	_clients.erase(it);
}

bool Server::isListenFd(int fd) const {
	return _listenOwners.find(fd) != _listenOwners.end();
}

void Server::run() {
	while (true) {
		rebuildPollFds();
		int ready = ::poll(&_pollFds[0], _pollFds.size(), -1);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() failed");
		}

		for (size_t i = 0; i < _pollFds.size(); ++i) {
			struct pollfd &p = _pollFds[i];
			if (p.revents == 0)
				continue;

			if (isListenFd(p.fd)) {
				if (p.revents & POLLIN)
					acceptClients(p.fd);
				continue;
			}

			if (p.revents & (POLLHUP | POLLERR | POLLNVAL)) {
				closeClient(p.fd);
				continue;
			}
			if (p.revents & POLLIN)
				handleClientRead(p.fd);
			if (p.revents & POLLOUT)
				handleClientWrite(p.fd);
		}
	}
}