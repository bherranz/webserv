#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include <string>
#include <poll.h>

#include "Client.hpp"

class Server {
public:
	explicit Server(int port);
	~Server();

	void run();

private:
	Server(const Server &);
	Server &operator=(const Server &);

	void initListenSocket();
	void setNonBlocking(int fd);

	void rebuildPollFds();
	void acceptClients();
	void handleClientRead(int fd);
	void handleClientWrite(int fd);
	void closeClient(int fd);

	int _port;
	int _listenFd;
	std::vector<struct pollfd> _pollFds;
	std::map<int, Client> _clients;
};

#endif
