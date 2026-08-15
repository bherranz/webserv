/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:54:20 by miparis           #+#    #+#             */
/*   Updated: 2026/03/31 17:35:00 by jaime            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

// Constructor: initializes the socket fd, the activity timestamp and keep-alive state.
Client::Client(int fd) : _fd(fd), _lastActivity(std::time(NULL)), _keepAlive(false) {}

// Returns the underlying client socket descriptor.
int Client::fd() const { return _fd; }

// Closes the socket if it is still open and marks the client as disconnected.
void Client::closeFd()
{
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}
// Provides mutable access to the input buffer where received request bytes are stored.
std::string &Client::inBuffer() { return _in; }
// Provides mutable access to the output buffer where pending response bytes are stored.
std::string &Client::outBuffer() { return _out; }

// Refreshes the last activity timestamp used by timeout checks.
void Client::updateActivity()
{
	_lastActivity = std::time(NULL);
}

// Returns the last time the client sent or received activity.
time_t Client::lastActivity() const
{
	return _lastActivity;
}

// Checks whether the client has stayed idle longer than the provided timeout.
bool Client::isTimedOut(time_t timeoutSec) const
{
	return std::time(NULL) - _lastActivity > timeoutSec;
}

// Stores whether this connection should remain open for another request.
void Client::setKeepAlive(bool val)
{
	_keepAlive = val;
}

// Returns the current keep-alive state for the connection.
bool Client::keepAlive() const
{
	return _keepAlive;
}

// Clears both request and response buffers when the connection is reused.
void Client::clearBuffers()
{
	_in.clear();
	_out.clear();
}
