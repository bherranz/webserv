/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:54:10 by miparis           #+#    #+#             */
/*   Updated: 2026/06/04 17:41:15 by miparis          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"
#include "Utils.hpp"

static volatile std::sig_atomic_t g_stopRequested = 0;

extern "C" void handleSigint(int) {
	g_stopRequested = 1;
}

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
		std::map<int, std::size_t>::iterator owner = _listenOwners.find(listenFd);
		if (owner != _listenOwners.end())
			_clientServers[clientFd] = owner->second;
	}
}

void Server::handleClientRead(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end()) {
		return;
	}

	char buf[4096];
	ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
	if (n > 0)
	{
		it->second.updateActivity();
		it->second.inBuffer().append(buf, n);

		HttpRequest &req = _pendingRequests[fd];
		bool headersJustCompleted = false;
		std::string &buffer = it->second.inBuffer();

		if (!req.headersComplete())
		{
			std::size_t headerEnd = buffer.find("\r\n\r\n");
			if (headerEnd != std::string::npos)
			{
				if (!req.parse(buffer))
				{
					HttpResponse response;
					response.setStatus(400, "Bad Request");
					response.setContentType("text/html");
					std::string body = "<html><body><h1>400 Bad Request</h1></body></html>";
					response.setBody(body);
					response.setContentLength(body.size());
					it->second.outBuffer() = response.toString();
					buffer.clear();
					_pendingRequests.erase(fd);
					return;
				}

				std::size_t headerBytes = headerEnd + 4;
				std::size_t bodyBytesConsumed = 0;
				std::map<std::string, std::string>::const_iterator contentLengthIt = req._headers.find("content-length");
				if (contentLengthIt != req._headers.end())
				{
					char *end = NULL;
					long bodyLen = std::strtol(contentLengthIt->second.c_str(), &end, 10);
					if (end != NULL && *end == '\0' && bodyLen >= 0)
					{
						std::size_t bodyAvailable = buffer.size() - headerBytes;
						std::size_t expectedBody = static_cast<std::size_t>(bodyLen);
						std::size_t bodyToConsume = bodyAvailable < expectedBody ? bodyAvailable : expectedBody;
						if (bodyToConsume > 0)
						{
							req.appendBodyData(buffer.substr(headerBytes, bodyToConsume));
							bodyBytesConsumed = bodyToConsume;
						}
					}
				}
				buffer.erase(0, headerBytes + bodyBytesConsumed);
				headersJustCompleted = true;
			}
			else
			{
				if (it->second.isTimedOut(getServerForClient(fd).clientTimeout))
				{
					HttpResponse response;
					response.setStatus(408, "Request Timeout");
					std::string body = "<html><body><h1>408 Request Timeout</h1></body></html>";
					response.setBody(body);
					response.setContentType("text/html");
					response.setContentLength(body.size());
					it->second.outBuffer() = response.toString();
					_pendingRequests.erase(fd);
					return;
				}
				return;
			}
		}

		if (req.headersComplete() && !req.bodyComplete())
		{
			if (!headersJustCompleted)
			{
				std::map<std::string, std::string>::const_iterator contentLengthIt = req._headers.find("content-length");
				if (contentLengthIt != req._headers.end())
				{
					char *end = NULL;
					long bodyLen = std::strtol(contentLengthIt->second.c_str(), &end, 10);
					if (end != NULL && *end == '\0' && bodyLen >= 0)
					{
						std::size_t bodyAvailable = buffer.size();
						std::size_t expectedBody = static_cast<std::size_t>(bodyLen);
						std::size_t bodyToConsume = bodyAvailable < expectedBody ? bodyAvailable : expectedBody;
						if (bodyToConsume > 0)
						{
							req.appendBodyData(buffer.substr(0, bodyToConsume));
							buffer.erase(0, bodyToConsume);
						}
					}
				}
			}
		}

		if (req.bodyComplete())
		{
			HttpResponse response;

			std::size_t serverIdx = 0;
			std::map<int, std::size_t>::iterator si = _clientServers.find(fd);
			if (si != _clientServers.end())
				serverIdx = si->second;

			std::map<std::string, std::string>::const_iterator hostIt = req._headers.find("Host");
			if (hostIt != req._headers.end())
			{
				std::string hostHeader = hostIt->second;
				std::size_t colonPos = hostHeader.find(':');
				if (colonPos != std::string::npos)
					hostHeader = hostHeader.substr(0, colonPos);

				bool found = false;
				for (std::size_t i = 0; i < _config.servers().size() && !found; ++i)
				{
					for (std::size_t j = 0; j < _config.servers()[i].serverNames.size() && !found; ++j)
					{
						if (_config.servers()[i].serverNames[j] == hostHeader)
						{
							serverIdx = i;
							found = true;
						}
					}
				}
			}

			Router router;
			router.route(req, response, _config.servers()[serverIdx]);

			response.setHeader("Server", "webserv/1.0");
			response.setHeader("Date", Utils::formatDate());

			bool keepAlive = shouldKeepAlive(req);
			it->second.setKeepAlive(keepAlive);
			if (keepAlive)
				response.setHeader("Connection", "keep-alive");
			else
				response.setHeader("Connection", "close");
			_pendingRequests.erase(fd);

			it->second.outBuffer() = response.toString();
			return;
		}
		return;
	}
	if (n <= 0)
	{
		_pendingRequests.erase(fd);
		closeClient(fd);
		return;
	}
}

void Server::handleClientWrite(int fd) {
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	std::string &out = it->second.outBuffer();
	if (out.empty())
		return;
	ssize_t n = ::send(fd, out.c_str(), out.size(), 0);
	if (n <= 0)
    {
        closeClient(fd);
        return;
    }

    out.erase(0, n);
    it->second.updateActivity();

    if (out.empty()) {
        if (it->second.keepAlive()) {
            it->second.clearBuffers();
            it->second.updateActivity();
        } else {
            closeClient(fd);
        }
    }
}

void Server::closeClient(int fd) {
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	it->second.closeFd();
	_clients.erase(it);
	_pendingRequests.erase(fd);
	_clientServers.erase(fd);
}

bool Server::isListenFd(int fd) const {
	return _listenOwners.find(fd) != _listenOwners.end();
}

const ServerConfig &Server::getServerForClient(int fd) const
{
	std::map<int, std::size_t>::const_iterator it = _clientServers.find(fd);
	std::size_t idx = 0;
	if (it != _clientServers.end())
		idx = it->second;
	if (idx >= _config.servers().size())
		idx = 0;
	return _config.servers()[idx];
}

bool Server::shouldKeepAlive(const HttpRequest &req) const
{
	std::map<std::string, std::string>::const_iterator it = req._headers.find("Connection");
	if (it != req._headers.end())
	{
		std::string val = it->second;
		for (std::size_t i = 0; i < val.size(); ++i)
			val[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(val[i])));
		if (val.find("keep-alive") != std::string::npos)
			return true;
	}
	return false;
}

void Server::checkTimeouts()
{
	std::map<int, Client>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		int fd = it->first;
		const ServerConfig &sc = getServerForClient(fd);
		time_t timeout = it->second.keepAlive() ? sc.keepaliveTimeout : sc.clientTimeout;

		if (it->second.isTimedOut(timeout))
		{
			if (it->second.outBuffer().empty() && _pendingRequests.find(fd) != _pendingRequests.end())
			{
				HttpResponse response;
				response.setStatus(408, "Request Timeout");
				std::string body = "<html><body><h1>408 Request Timeout</h1></body></html>";
				response.setBody(body);
				response.setContentType("text/html");
				response.setContentLength(body.size());
				it->second.outBuffer() = response.toString();
				_pendingRequests.erase(fd);
			}
			it->second.closeFd();
			_clientServers.erase(fd);
			_pendingRequests.erase(fd);
			_clients.erase(it++);
		}
		else
		{
			++it;
		}
	}
}

void Server::run() {
	static const int pollTimeoutMs = 1000;
	std::signal(SIGINT, handleSigint);
	std::signal(SIGPIPE, SIG_IGN);
	while (!g_stopRequested) {
		rebuildPollFds();
		int ready = ::poll(&_pollFds[0], _pollFds.size(), pollTimeoutMs);
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
		checkTimeouts();
	}
	std::cout << "Shutting down server..." << std::endl;
}
