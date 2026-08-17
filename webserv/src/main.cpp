#include <iostream>
#include "Config.hpp"
#include "Server.hpp"

int main(int argc, char **argv)
{
    std::string config_file;

    if (argc > 2)
    {
        std::cerr << "Usage: ./webserv [config_file]\n";
        return (1);
    }
    if (argc == 2)
        config_file = argv[1];
    else
        config_file = "config/default.conf";
    try
    {
        // Load and parse config file
        Config config;
        config.load(config_file);

		std::cout << BLUE << "SERVER READY " << NC << std::endl;
        // Init server
        Server server(config);
		server.run();
	}
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
