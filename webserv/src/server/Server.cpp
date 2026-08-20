#include "Server.hpp"

static volatile std::sig_atomic_t g_stopRequested = 0;

// SIGINT handler: marks the event loop for shutdown without doing unsafe work inside the signal context.
extern "C" void handleSigint(int)
{
	g_stopRequested = 1;
}

// Returns true when the socket should bind in passive mode for wildcard addresses.
static bool shouldUsePassive(const std::string &host)
{
	return host.empty() || host == "0.0.0.0" || host == "::";
}

// Detects a wildcard host so one listen socket can win over a more specific binding on the same port.
static bool isWildcardHost(const std::string &host)
{
	return host.empty() || host == "0.0.0.0";
}

// Stores the config and opens all listening sockets before the loop starts.
Server::Server(const Config &config) : _config(config) { initListenSockets(); }

// Closes every open client and listener descriptor when the server object is destroyed.
Server::~Server()
{
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		it->second.closeFd();
	for (std::size_t i = 0; i < _listenFds.size(); ++i)
		::close(_listenFds[i]);
}

void Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl(F_SETFL) failed");
}

void Server::initListenSockets() {
	std::map<int, std::pair<ListenConfig, std::size_t> > chosenByPort;
	for (std::size_t serverIndex = 0; serverIndex < _config.servers().size(); ++serverIndex) {
		const ServerConfig &server = _config.servers()[serverIndex];
		for (std::size_t listenIndex = 0; listenIndex < server.listens.size(); ++listenIndex)
		{
			const ListenConfig &listenConfig = server.listens[listenIndex];
			std::map<int, std::pair<ListenConfig, std::size_t> >::iterator chosen = chosenByPort.find(listenConfig.port);
			if (chosen == chosenByPort.end() || (isWildcardHost(listenConfig.host) && !isWildcardHost(chosen->second.first.host)))
				chosenByPort[listenConfig.port] = std::make_pair(listenConfig, serverIndex);
		}
	}
	for (std::map<int, std::pair<ListenConfig, std::size_t> >::const_iterator it = chosenByPort.begin(); it != chosenByPort.end(); ++it)
		openListenSocket(it->second.first, it->second.second);
	if (_listenFds.empty())
		throw std::runtime_error("configuration does not define any listen endpoints");
}

// Creates, binds and listens on one TCP socket for a single listen directive.
void Server::openListenSocket(const ListenConfig &listenConfig, std::size_t serverIndex)
{
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = shouldUsePassive(listenConfig.host) ? AI_PASSIVE : 0;

	struct addrinfo *result = NULL;
	const char *host = shouldUsePassive(listenConfig.host) ? NULL : listenConfig.host.c_str();
	int err = ::getaddrinfo(host, Utils::toString(listenConfig.port).c_str(), &hints, &result);
	if (err != 0)
	{
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
		if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
		{
			::close(listenFd);
			listenFd = -1;
			continue;
		}
		try
		{
			setNonBlocking(listenFd);
		}
		catch (...)
		{
			::close(listenFd);
			listenFd = -1;
			continue;
		}
		if (::bind(listenFd, current->ai_addr, current->ai_addrlen) < 0)
		{
			::close(listenFd);
			listenFd = -1;
			continue;
		}
		if (::listen(listenFd, SOMAXCONN) < 0)
		{
			::close(listenFd);
			listenFd = -1;
			continue;
		}
		break;
	}
	::freeaddrinfo(result);
	if (listenFd < 0)
	{
		std::ostringstream message;
		message << "unable to open listen socket on " << listenConfig.host << ':' << listenConfig.port;
		throw std::runtime_error(message.str());
	}
	_listenFds.push_back(listenFd);
	_listenOwners[listenFd] = serverIndex;
	_listenPorts[listenFd] = listenConfig.port;
	std::cout << "Listening on http://" << listenConfig.host << ':' << listenConfig.port << std::endl;
}

// Rebuilds the poll() set with listeners, connected clients and active CGI pipes.
void Server::rebuildPollFds() {
	_pollFds.clear();
	for (std::size_t i = 0; i < _listenFds.size(); ++i)
	{
		struct pollfd listenPollFd;
		std::memset(&listenPollFd, 0, sizeof(listenPollFd));
		listenPollFd.fd = _listenFds[i];
		listenPollFd.events = POLLIN;
		_pollFds.push_back(listenPollFd);
	}
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		struct pollfd c;
		std::memset(&c, 0, sizeof(c));
		c.fd = it->first;
		c.events = POLLIN;
		if (!it->second.outBuffer().empty())
			c.events |= POLLOUT;
		_pollFds.push_back(c);
	}
	for (std::map<int, CgiTask>::iterator it = _activeCgis.begin(); it != _activeCgis.end(); ++it) {
		// read from the CGI stdout pipe
		struct pollfd pfdOut;
		std::memset(&pfdOut, 0, sizeof(pfdOut));
		pfdOut.fd = it->second.stdoutFd;
		pfdOut.events = POLLIN;
		_pollFds.push_back(pfdOut);

		// write to the CGI stdin pipe if there's data to send
		if (it->second.stdinFd >= 0 && it->second.writeOffset < it->second.inputData.size()) {
			struct pollfd pfdIn;
			std::memset(&pfdIn, 0, sizeof(pfdIn));
			pfdIn.fd = it->second.stdinFd;
			pfdIn.events = POLLOUT;
			_pollFds.push_back(pfdIn);
		}
	}
}

// Accepts every pending client queued on a listening socket and switches it to non-blocking mode.
void Server::acceptClients(int listenFd) {
	while (true) {
		int clientFd = ::accept(listenFd, NULL, NULL);
		if (clientFd < 0)
			return;
		try
		{
			setNonBlocking(clientFd);
		}
		catch (...)
		{
			::close(clientFd);
			continue;
		}
		_clients.insert(std::make_pair(clientFd, Client(clientFd)));
		std::map<int, std::size_t>::iterator owner = _listenOwners.find(listenFd);
		if (owner != _listenOwners.end())
			_clientServers[clientFd] = owner->second;
		std::map<int, int>::iterator portIt = _listenPorts.find(listenFd);
		if (portIt != _listenPorts.end())
			_clientPorts[clientFd] = portIt->second;
	}
}

// Appends network data to the request buffer, parses headers and routes the request when complete.
void Server::handleClientRead(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	char buf[4096];
	ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
	if (n > 0)
	{
		it->second.updateActivity();
		it->second.inBuffer().append(buf, n);
		processClientRequests(fd);
		return;
	}
	closeClient(fd);
}

void Server::processClientRequests(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	// Do not process next request if previous response is still in outBuffer
	if (!it->second.outBuffer().empty())
		return;

	// Do not process next request if a CGI task is already running for this client
	for (std::map<int, CgiTask>::const_iterator cgiIt = _activeCgis.begin(); cgiIt != _activeCgis.end(); ++cgiIt)
	{
		if (cgiIt->second.clientFd == fd)
			return;
	}

	HttpRequest &req = _pendingRequests[fd];
	std::string &buffer = it->second.inBuffer();

	if (!req.headersComplete())
	{
		std::size_t headerEnd = buffer.find("\r\n\r\n");
		if (headerEnd != std::string::npos)
		{
			std::size_t headerBytes = headerEnd + 4;
			std::string headerPart = buffer.substr(0, headerBytes);
			if (!req.parse(headerPart))
			{
				HttpResponse response;
				response.setError(400);
				it->second.outBuffer() = response.toString();
				buffer.clear();
				_pendingRequests.erase(fd);
				return;
			}

			std::size_t bodyBytesConsumed = 0;
			if (req.isChunked())
			{
				std::size_t bodyAvailable = buffer.size() - headerBytes;
				if (bodyAvailable > 0)
				{
					req.appendBodyData(buffer.substr(headerBytes));
					bodyBytesConsumed = bodyAvailable;
				}
			}
			else
			{
				std::map<std::string, std::string>::const_iterator contentLengthIt = req._headers.find("content-length");
				if (contentLengthIt != req._headers.end())
				{
					char *end = NULL;
					long bodyLen = std::strtol(contentLengthIt->second.c_str(), &end, 10);
					if (end != NULL && *end == '\0' && bodyLen >= 0)
					{
						std::size_t bodyAvailable = buffer.size() - headerBytes;
						std::size_t expectedBody = static_cast<std::size_t>(bodyLen);
						std::size_t bodyToConsume = (bodyAvailable < expectedBody) ? bodyAvailable : expectedBody;
						if (bodyToConsume > 0)
						{
							req.appendBodyData(buffer.substr(headerBytes, bodyToConsume));
							bodyBytesConsumed = bodyToConsume;
						}
					}
				}
			}
			buffer.erase(0, headerBytes + bodyBytesConsumed);
		}
		else
		{
			if (it->second.isTimedOut(getServerForClient(fd).clientTimeout))
			{
				HttpResponse response;
				response.setError(408);
				it->second.outBuffer() = response.toString();
				buffer.clear();
				_pendingRequests.erase(fd);
				return;
			}
			return;
		}
	}
	else
	{
		if (!buffer.empty())
		{
			if (req.isChunked())
			{
				req.appendBodyData(buffer);
				buffer.clear();
			}
			else
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
						std::size_t remainingBody = (expectedBody > req._body.size()) ? (expectedBody - req._body.size()) : 0;
						std::size_t bodyToConsume = (bodyAvailable < remainingBody) ? bodyAvailable : remainingBody;
						if (bodyToConsume > 0)
						{
							req.appendBodyData(buffer.substr(0, bodyToConsume));
							buffer.erase(0, bodyToConsume);
						}
					}
				}
			}
		}
	}

	bool isComplete = false;
	try
	{
		isComplete = req.bodyComplete();
	}
	catch (const std::exception &e)
	{
		HttpResponse response;
		response.setError(400);
		it->second.outBuffer() = response.toString();
		buffer.clear();
		_pendingRequests.erase(fd);
		return;
	}

	if (isComplete)
	{
		HttpResponse response;

		std::size_t serverIdx = 0;
		std::map<int, std::size_t>::iterator si = _clientServers.find(fd);
		if (si != _clientServers.end())
			serverIdx = si->second;

		int clientPort = 8080;
		std::map<int, int>::iterator cp = _clientPorts.find(fd);
		if (cp != _clientPorts.end())
			clientPort = cp->second;

		std::map<std::string, std::string>::const_iterator hostIt = req._headers.find("host");
		if (hostIt != req._headers.end())
		{
			std::string hostHeader = hostIt->second;
			std::size_t colonPos = hostHeader.find(':');
			if (colonPos != std::string::npos)
				hostHeader = hostHeader.substr(0, colonPos);

			bool found = false;
			for (std::size_t i = 0; i < _config.servers().size() && !found; ++i)
			{
				bool listensOnPort = false;
				for (std::size_t k = 0; k < _config.servers()[i].listens.size(); ++k)
				{
					if (_config.servers()[i].listens[k].port == clientPort)
					{
						listensOnPort = true;
						break;
					}
				}
				if (listensOnPort)
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
		}

		Router router;
		CgiFds cgi = router.route(req, response, _config.servers()[serverIdx]);

		bool keepAlive = shouldKeepAlive(req);
		it->second.setKeepAlive(keepAlive);

		if (cgi.pid > 0)
		{
			CgiTask task;
			task.pid = cgi.pid;
			task.stdoutFd = cgi.readFd;
			task.stdinFd = cgi.writeFd;
			task.clientFd = fd;
			task.outputData = "";
			task.inputData = req._body;
			task.writeOffset = 0;
			task.startTime = std::time(NULL);
			if (task.inputData.empty())
			{
				if (task.stdinFd >= 0)
				{
					::close(task.stdinFd);
					task.stdinFd = -1;
				}
			}
			_activeCgis[cgi.readFd] = task;
			_pendingRequests.erase(fd);
			return;
		}

		if (keepAlive)
			response.setHeader("Connection", "keep-alive");
		else
			response.setHeader("Connection", "close");

		_pendingRequests.erase(fd);
		it->second.outBuffer() = response.toString();
	}
}

// Sends the current response buffer and decides whether to keep the socket alive or close it.
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
			it->second.updateActivity();
			if (!it->second.inBuffer().empty())
				processClientRequests(fd);
		} else {
			closeClient(fd);
		}
	}
}

// Closes one client connection and removes any CGI task that was spawned for it.
void Server::closeClient(int fd) {
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	// Clean up any active CGI tasks associated with this client
	std::map<int, CgiTask>::iterator cgiIt = _activeCgis.begin();
	while (cgiIt != _activeCgis.end())
	{
		if (cgiIt->second.clientFd == fd)
		{
			kill(cgiIt->second.pid, SIGKILL);
			int status;
			waitpid(cgiIt->second.pid, &status, WNOHANG);
			if (cgiIt->second.stdinFd >= 0)
				::close(cgiIt->second.stdinFd);
			::close(cgiIt->second.stdoutFd);
			_activeCgis.erase(cgiIt++);
		}
		else
			++cgiIt;
	}

	it->second.closeFd();
	_clients.erase(it);
	_pendingRequests.erase(fd);
	_clientServers.erase(fd);
	_clientPorts.erase(fd);
}

bool Server::isListenFd(int fd) const
{
	// Fast lookup used by the poll loop to decide whether an fd is a listener.
	return _listenOwners.find(fd) != _listenOwners.end();
}

// Returns the server block that originally accepted this client connection.
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

// Checks the Connection header to decide whether the next response can reuse the socket.
bool Server::shouldKeepAlive(const HttpRequest &req) const
{
	std::map<std::string, std::string>::const_iterator it = req._headers.find("connection");
	if (it != req._headers.end())
	{
		std::string val = it->second;
		for (std::size_t i = 0; i < val.size(); ++i)
			val[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(val[i])));
		if (val.find("close") != std::string::npos)
			return false;
		if (val.find("keep-alive") != std::string::npos)
			return true;
	}
	if (req._version == "HTTP/1.1")
		return true;
	return false;
}

// Periodic timeout entry point that splits CGI and client timeout handling.
void Server::checkTimeouts()
{
	checkCgiTimeouts();
	checkClientTimeouts();
}

// Kills CGI processes that exceeded the 5 second execution window and replies with 504.
void Server::checkCgiTimeouts()
{
	std::map<int, CgiTask>::iterator cgiIt = _activeCgis.begin();
	while (cgiIt != _activeCgis.end())
	{
		if (std::time(NULL) - cgiIt->second.startTime > 5)
		{
			kill(cgiIt->second.pid, SIGKILL);

			int status;
			waitpid(cgiIt->second.pid, &status, WNOHANG);

			int clientFd = cgiIt->second.clientFd;
			std::map<int, Client>::iterator clientIt = _clients.find(clientFd);
			if (clientIt != _clients.end())
			{
				HttpResponse response;
				response.setError(504, "CGI process took too long");
				response.setHeader("Connection", "close");
				clientIt->second.setKeepAlive(false);
				clientIt->second.outBuffer() = response.toString();
				clientIt->second.updateActivity();
			}
			if (cgiIt->second.stdinFd >= 0)
				::close(cgiIt->second.stdinFd);
			::close(cgiIt->second.stdoutFd);
			_activeCgis.erase(cgiIt++);
		}
		else
			++cgiIt;
	}
}

// Closes clients that stayed idle beyond their configured timeout value.
void Server::checkClientTimeouts()
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
				response.setError(408);
				response.setHeader("Connection", "close");
				it->second.setKeepAlive(false);
				it->second.outBuffer() = response.toString();
				_pendingRequests.erase(fd);
			}
			it->second.closeFd();
			_clientServers.erase(fd);
			_pendingRequests.erase(fd);
			_clients.erase(it++);
		}
		else
			++it;
	}
}

// Main event loop: polls all descriptors, dispatches events and keeps timeouts under control.
void Server::run() {
	static const int pollTimeoutMs = 1000;
	std::signal(SIGINT, handleSigint);
	std::signal(SIGPIPE, SIG_IGN);
	while (!g_stopRequested)
	{
		rebuildPollFds();
		int ready = ::poll(&_pollFds[0], _pollFds.size(), pollTimeoutMs);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() failed");
		}
		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			struct pollfd &p = _pollFds[i];
			if (p.revents == 0)
				continue;
			if (isListenFd(p.fd))
			{
				if (p.revents & POLLIN)
					acceptClients(p.fd);
				continue;
			}
			if (handleCgiEvent(p))
				continue;
			if (_clients.find(p.fd) != _clients.end())
			{
				if (p.revents & (POLLHUP | POLLERR | POLLNVAL))
				{
					closeClient(p.fd);
					continue;
				}
				if (p.revents & POLLIN)
					handleClientRead(p.fd);
				if (p.revents & POLLOUT)
					handleClientWrite(p.fd);
			}
		}
		checkTimeouts();
	}
	std::cout << "Shutting down server..." << std::endl;
}

// Routes poll events for CGI stdin/stdout pipes so CGI work can progress without blocking.
bool Server::handleCgiEvent(struct pollfd &p)
{
	for (std::map<int, CgiTask>::iterator cgiIt = _activeCgis.begin();
			cgiIt != _activeCgis.end(); ++cgiIt)
	{
		// if the event is on the CGI stdin pipe
		if (cgiIt->second.stdinFd >= 0 && p.fd == cgiIt->second.stdinFd)
		{
			if (p.revents & (POLLOUT | POLLERR | POLLHUP))
				handleCgiWrite(cgiIt->second);
			return true;
		}
		// if the event is on the CGI stdout pipe
		if (p.fd == cgiIt->second.stdoutFd)
		{
			if (p.revents & (POLLIN | POLLHUP | POLLERR))
				handleCgiRead(cgiIt);
			return true;
		}
	}
	return false;
}

// Streams request body data into the CGI process through its stdin pipe.
void Server::handleCgiWrite(CgiTask &task)
{
	if (task.stdinFd < 0)
		return;
	if (task.writeOffset < task.inputData.size())
	{
		ssize_t n = ::write(task.stdinFd, task.inputData.c_str() + task.writeOffset,
							task.inputData.size() - task.writeOffset);
		if (n > 0)
			task.writeOffset += n;
		else
		{
			::close(task.stdinFd);
			task.stdinFd = -1;
			return;
		}
	}
	// If all data has been written, close the stdin pipe so child receives EOF
	if (task.writeOffset >= task.inputData.size())
	{
		if (task.stdinFd >= 0)
		{
			::close(task.stdinFd);
			task.stdinFd = -1;
		}
	}
}

// Reads CGI stdout, finalizes the CGI response and stores it in the target client buffer.
void Server::handleCgiRead(std::map<int, CgiTask>::iterator &cgiIt)
{
	char buf[4096];
	ssize_t n = ::read(cgiIt->second.stdoutFd, buf, sizeof(buf));

	if (n > 0)
	{
		cgiIt->second.outputData.append(buf, n);
		return;
	}

	// EOF or error on CGI stdout
	int status = 0;
	pid_t ret = waitpid(cgiIt->second.pid, &status, WNOHANG);
	if (ret == 0)
	{
		kill(cgiIt->second.pid, SIGKILL);
		waitpid(cgiIt->second.pid, &status, WNOHANG);
	}

	// Parse the CGI output and build the final HttpResponse
	HttpResponse response;
	bool hasError = (WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0));
	if (hasError && cgiIt->second.outputData.empty())
		response.setError(500, "CGI script exited with error");
	else if (!CGIHandler::finalize(cgiIt->second.outputData, response))
		response.setError(500, "CGI output malformed");

	// Search for the client associated with this CGI task and send the response
	int clientFd = cgiIt->second.clientFd;
	std::map<int, Client>::iterator clientIt = _clients.find(clientFd);
	if (clientIt != _clients.end())
	{
		bool keepAlive = clientIt->second.keepAlive();
		if (keepAlive)
			response.setHeader("Connection", "keep-alive");
		else
			response.setHeader("Connection", "close");

		clientIt->second.outBuffer() = response.toString();
		clientIt->second.updateActivity();
	}

	// Clean up the CGI task
	if (cgiIt->second.stdinFd >= 0)
		::close(cgiIt->second.stdinFd);
	::close(cgiIt->second.stdoutFd);
	_activeCgis.erase(cgiIt);
}
