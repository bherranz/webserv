#include "ConfigParser.hpp"
#include "Config.hpp"

bool ConfigParser::isServerStart(const std::vector<std::string>& tokens) const
{
    return (tokens.size() == 2 && tokens[0] == "server" && tokens[1] == "{");
}

bool ConfigParser::isLocationStart(const std::vector<std::string>& tokens) const
{
    return (tokens.size() >= 3 && tokens[0] == "location" && tokens.back() == "{");
}

bool ConfigParser::isBlockEnd(const std::vector<std::string>& tokens) const
{
    return (tokens.size() == 1 && tokens[0] == "}");
}

void ConfigParser::parseServerBlock(const std::vector<std::string>& lines, std::size_t& i, std::vector<ServerConfig>& servers)
	{
	ServerConfig newServer;

	//We start from "server {" or similar an get into the first data block
	i++;

	while (i < lines.size())
	{
		std::vector<std::string> tokens = tokenize(lines[i]);
		
		if (tokens.empty())
		{
			i++;
			continue;
		}

		if (isBlockEnd(tokens))
		{
			// if we find '}' means there is no data left, hence we just close the server
			// we saved the nwe config sercer and out
			servers.push_back(newServer);
			return;
		}
		else if (isLocationStart(tokens)) //as location blogs are handled specifically, we call other function
			parseLocationBlock(lines, i, newServer);
		else
		{
			// Ej: "listen 8080;", "server_name localhost;", "root /var/www;"
			std::cout << "  -> Directiva de Server a parsear: " << tokens[0] << std::endl;

			// Search for the 1º token (ej: "listen", "root") in our map
			std::map<std::string, ServerDirectiveFn>::iterator it = _serverParsers.find(tokens[0]);
			
			if (it != _serverParsers.end())
				// We pass this token lines to our 'newServer'
				(this->*(it->second))(tokens, newServer);
			else
				throw std::runtime_error("config error: unknown server directive '" + tokens[0] + "'");
		}
		i++;
	}
	throw std::runtime_error("config error: unclosed server block"); // If we endend without a '}', the config is not closed
}

void ConfigParser::parseLocationBlock(const std::vector<std::string>& lines, std::size_t& i, ServerConfig& server)
{
	/*
	** Parsing 'location /path { ... }'.
	** We pass the ServerConfig by reference to add the path at the end
	*/

    LocationConfig newLocation;

    std::vector<std::string> startTokens = tokenize(lines[i]);
    if (startTokens.size() >= 2)
	{
		// tokens[0] = "location", tokens[1] = "/path", tokens[2] = "{"
        newLocation.path = startTokens[1]; // We save the route
        std::cout << "    -> Entrando en Location: " << startTokens[1] << std::endl;
    }
    
    i++; //inside the location block

    while (i < lines.size())
    {
        std::vector<std::string> tokens = tokenize(lines[i]);
        
        if (tokens.empty())
        {
            i++;
            continue;
        }

        if (isBlockEnd(tokens))
        {
             // if we find '}' means there is no data left, hence we just close the location block
            // we saved the data for the  location and out
            server.locations.push_back(newLocation);
            return;
        }
        else
        {
            // Ej: "allowed_methods GET POST;", "autoindex on;"
            std::cout << "      -> Directiva de Location a parsear: " << tokens[0] << std::endl;

            std::map<std::string, LocationDirectiveFn>::iterator it = _locationParsers.find(tokens[0]);
            
            if (it != _locationParsers.end())
                (this->*(it->second))(tokens, newLocation);
            else
                throw std::runtime_error("config error: unknown location directive '" + tokens[0] + "'");
        }
        i++;
    }

    throw std::runtime_error("config error: unclosed location block");
}

void Config::print() const
{
    std::cout << "\n=======================================================" << std::endl;
    std::cout << "      RESULTADO DEL PARSEO DE CONFIGURACIÓN            " << std::endl;
    std::cout << "=======================================================" << std::endl;
    std::cout << "Servidores encontrados: " << _servers.size() << "\n" << std::endl;

    for (std::size_t i = 0; i < _servers.size(); ++i)
    {
        const ServerConfig& srv = _servers[i];
        
        std::cout << "[ SERVER #" << (i + 1) << " ]" << std::endl;
        
        // --- Listens ---
        std::cout << "  |- Listens (" << srv.listens.size() << "):" << std::endl;
        for (std::size_t j = 0; j < srv.listens.size(); ++j) {
            std::cout << "  |  - " << srv.listens[j].host << ":" << srv.listens[j].port << std::endl;
        }

        // --- Server Names ---
        if (!srv.serverNames.empty()) {
            std::cout << "  |- Server Names: ";
            for (std::size_t j = 0; j < srv.serverNames.size(); ++j)
                std::cout << srv.serverNames[j] << " ";
            std::cout << std::endl;
        }

        // --- Variables Globales ---
        if (srv.hasRoot)
            std::cout << "  |- Root: " << srv.root << std::endl;
        
        if (srv.hasIndex) {
            std::cout << "  |- Index: ";
            for (std::size_t j = 0; j < srv.index.size(); ++j)
                std::cout << srv.index[j] << " ";
            std::cout << std::endl;
        }

        if (srv.hasClientMaxBodySize)
            std::cout << "  |- Client Max Body Size: " << srv.clientMaxBodySize << std::endl;

        // --- Error Pages (Mapa) ---
        if (!srv.errorPages.empty()) {
            std::cout << "  |- Error Pages:" << std::endl;
            // En C++98 necesitamos un iterador constante para leer un mapa const
            std::map<int, std::string>::const_iterator it = srv.errorPages.begin();
            for (; it != srv.errorPages.end(); ++it) {
                std::cout << "  |  - Error " << it->first << " -> " << it->second << std::endl;
            }
        }

        // --- Locations ---
        std::cout << "  |- Locations (" << srv.locations.size() << "):" << std::endl;
        for (std::size_t j = 0; j < srv.locations.size(); ++j)
        {
            const LocationConfig& loc = srv.locations[j];
            std::cout << "     [ Location " << loc.path << " ]" << std::endl;

            if (loc.hasAllowMethods) {
                std::cout << "       |- Allow Methods: ";
                for (std::size_t k = 0; k < loc.allowMethods.size(); ++k)
                    std::cout << loc.allowMethods[k] << " ";
                std::cout << std::endl;
            }
            
            if (loc.hasAutoindex)
                std::cout << "       |- Autoindex: " << (loc.autoindex ? "ON" : "OFF") << std::endl;
                
            if (loc.hasRoot)
                std::cout << "       |- Root: " << loc.root << std::endl;
                
            if (loc.hasUploadStore)
                std::cout << "       |- Upload Store: " << loc.uploadStore << std::endl;

            if (!loc.cgiPath.empty()) {
                std::cout << "       |- CGI Paths: ";
                for (std::size_t k = 0; k < loc.cgiPath.size(); ++k)
                    std::cout << loc.cgiPath[k] << " ";
                std::cout << std::endl;
            }

            if (!loc.cgiExt.empty()) {
                std::cout << "       |- CGI Exts: ";
                for (std::size_t k = 0; k < loc.cgiExt.size(); ++k)
                    std::cout << loc.cgiExt[k] << " ";
                std::cout << std::endl;
            }

            if (loc.hasRedirect)
                std::cout << "       |- Redirect: " << loc.redirectCode << " -> " << loc.redirectTarget << std::endl;
        }
        std::cout << "-------------------------------------------------------" << std::endl;
    }
}