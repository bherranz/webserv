#include "Config.hpp"

void Config::validate() const
{
	for (std::size_t i = 0; i < _servers.size(); ++i)
	{
		const ServerConfig& srv = _servers[i];

		// 1. Validar Puertos
		for (std::size_t j = 0; j < srv.listens.size(); ++j) {
			if (srv.listens[j].port < 1 || srv.listens[j].port > 65535)
				throw std::runtime_error("validation error: port out of range (1-65535)");
		}

		// 2. Validar Timeouts (si son muy absurdos o negativos, aunque atol los hace positivos, 
		// pero podemos evitar que sean 0 para no colgar el poll)
		if (srv.clientTimeout <= 0 || srv.keepaliveTimeout <= 0)
			throw std::runtime_error("validation error: timeouts must be greater than 0");

		// 3. Validar Error Pages
		std::map<int, std::string>::const_iterator it = srv.errorPages.begin();
		for (; it != srv.errorPages.end(); ++it) {
			if (it->first < 300 || it->first > 599)
				throw std::runtime_error("validation error: error_page code must be between 300 and 599");
		}

		// 4. Validar Locations
		for (std::size_t j = 0; j < srv.locations.size(); ++j)
		{
			const LocationConfig& loc = srv.locations[j];

			// Validar métodos permitidos
			for (std::size_t k = 0; k < loc.allowMethods.size(); ++k) {
				const std::string& method = loc.allowMethods[k];
				if (method != "GET" && method != "POST" && method != "DELETE")
					throw std::runtime_error("validation error: invalid allow_method '" + method + "'");
			}

			// Validar códigos de redirección
			if (loc.hasRedirect) {
				if (loc.redirectCode != 301 && loc.redirectCode != 302 && 
					loc.redirectCode != 303 && loc.redirectCode != 307 && loc.redirectCode != 308)
					throw std::runtime_error("validation error: invalid redirect code");
			}
		}
	}
}