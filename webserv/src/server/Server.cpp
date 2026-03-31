/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:54:10 by miparis           #+#    #+#             */
/*   Updated: 2026/03/31 17:35:00 by jaime            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Server::Server(int port) : _port(port), _listenFd(-1) { initListenSocket(); }

Server::~Server() {
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		it->second.closeFd();
	if (_listenFd >= 0)
		::close(_listenFd);
}

void Server::setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl(F_SETFL) failed");
}

void Server::initListenSocket() {
	_listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error("socket() failed");

	int yes = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

	setNonBlocking(_listenFd);

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (::bind(_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
		std::string msg = "bind() failed: ";
		msg += std::strerror(errno);
		throw std::runtime_error(msg);
	}

	if (::listen(_listenFd, SOMAXCONN) < 0)
		throw std::runtime_error("listen() failed");

	std::cout << "Listening on http://localhost:" << _port << std::endl;
}

void Server::rebuildPollFds() {
	_pollFds.clear();

	struct pollfd p;
	std::memset(&p, 0, sizeof(p));
	p.fd = _listenFd;
	p.events = POLLIN;
	_pollFds.push_back(p);

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

void Server::acceptClients() {
	while (true) {
		int clientFd = ::accept(_listenFd, NULL, NULL);
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

void Server::handleClientRead(int fd) {
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	char buf[4096];
	while (true) {
		ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
		if (n > 0) {
			it->second.inBuffer().append(buf, n);
			if (it->second.outBuffer().empty()) {
				it->second.outBuffer() = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
			}
			continue;
		}
		if (n == 0) {
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

			if (p.fd == _listenFd) {
				if (p.revents & POLLIN)
					acceptClients();
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