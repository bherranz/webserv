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

#include <unistd.h>

Client::Client(int fd) : _fd(fd) {}

int Client::fd() const { return _fd; }

void Client::closeFd() {
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}

std::string &Client::inBuffer() { return _in; }
std::string &Client::outBuffer() { return _out; }