#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include <string>
#include <poll.h>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <sstream>
#include <map>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <csignal>

#include "Config.hpp"
#include "Client.hpp"
#include "HttpRequest.hpp"

class Server {
public:
	explicit Server(const Config &config);
	~Server();

	void run();

private:
	Server(const Server &);
	Server &operator=(const Server &);

	void initListenSockets();
	void openListenSocket(const ListenConfig &listenConfig, std::size_t serverIndex);
	void setNonBlocking(int fd);

	void rebuildPollFds();
	void acceptClients(int listenFd);
	void handleClientRead(int fd);
	void handleClientWrite(int fd);
	void closeClient(int fd);
	bool isListenFd(int fd) const;

	void checkTimeouts();
	bool shouldKeepAlive(const HttpRequest &req) const;
	const ServerConfig &getServerForClient(int fd) const;

	Config _config;
	std::vector<int> _listenFds;
	std::map<int, std::size_t> _listenOwners;
	std::vector<struct pollfd> _pollFds;
	std::map<int, Client> _clients;
	std::map<int, std::size_t> _clientServers;
	std::map<int, HttpRequest> _pendingRequests;
};

#endif
