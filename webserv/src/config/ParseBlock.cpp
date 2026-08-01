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

void ConfigParser::parseServerBlock(const std::vector<std::vector<std::string> >& statements, std::size_t& i, std::vector<ServerConfig>& servers)
{
	ServerConfig newServer;

	i++;
	
	while (i < statements.size())
	{
		const std::vector<std::string>& tokens = statements[i]; // Ya están separados
		
		if (tokens.empty())
		{
			i++;
			continue;
		}

		if (isBlockEnd(tokens))
		{
			servers.push_back(newServer);
			return;
		}
		else if (isLocationStart(tokens))
			parseLocationBlock(statements, i, newServer);
		else
		{
			std::map<std::string, ServerDirectiveFn>::iterator it = _serverParsers.find(tokens[0]);
			
			if (it != _serverParsers.end())
				(this->*(it->second))(tokens, newServer);
			else
				throw std::runtime_error("config error: unknown server directive '" + tokens[0] + "'");
		}
		i++;
	}
	throw std::runtime_error("config error: unclosed server block");
}

void ConfigParser::parseLocationBlock(const std::vector<std::vector<std::string> >& statements, std::size_t& i, ServerConfig& server)
{
    LocationConfig newLocation;

    const std::vector<std::string>& startTokens = statements[i];

    if (startTokens.size() >= 2)
        newLocation.path = startTokens[1];
    i++;

    while (i < statements.size())
    {
        const std::vector<std::string>& tokens = statements[i];
        if (tokens.empty())
        {
            i++;
            continue;
        }
        if (isBlockEnd(tokens))
        {
            server.locations.push_back(newLocation);
            return;
        }
        else
        {
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