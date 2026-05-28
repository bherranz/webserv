/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jaime <jaime@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/31 16:46:31 by jaime             #+#    #+#             */
/*   Updated: 2026/05/28 13:05:01 by jaime            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>

#include "ConfigParser.hpp"
#include "Server.hpp"

int main(int argc, char **argv) {
	try {
		std::string configPath = "web.config";
		if (argc > 1)
			configPath = argv[1];
		Config config = ConfigParser::parseFile(configPath);
		Server server(config);
		server.run();
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
}