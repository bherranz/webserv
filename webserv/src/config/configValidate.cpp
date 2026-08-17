#include "Config.hpp"

void Config::validate() const
{
	const std::size_t ABSOLUTE_MAX_BODY_SIZE = 100 * 1024 * 1024;

	for (std::size_t i = 0; i < _servers.size(); ++i)
	{
		const ServerConfig& srv = _servers[i];

		if (srv.hasClientMaxBodySize && srv.clientMaxBodySize > ABSOLUTE_MAX_BODY_SIZE)
            throw std::runtime_error("config error: server client_max_body_size exceeds the absolute safe limit of 100MB");

		for (std::size_t j = 0; j < srv.locations.size(); ++j)
        {
            const LocationConfig& loc = srv.locations[j];

            if (loc.hasClientMaxBodySize && loc.clientMaxBodySize > ABSOLUTE_MAX_BODY_SIZE)
                throw std::runtime_error("config error: location client_max_body_size exceeds the absolute safe limit of 100MB");
		}
		// 1. Validate ports
		for (std::size_t j = 0; j < srv.listens.size(); ++j)
		{
			if (srv.listens[j].port < 1 || srv.listens[j].port > 65535)
				throw std::runtime_error("config error: port out of range (1-65535)");
		}

		// 2. Validate Timeouts
		if (srv.clientTimeout <= 0 || srv.keepaliveTimeout <= 0)
			throw std::runtime_error("config error: timeouts must be greater than 0");

		// 3. Validate Error Pages
		std::map<int, std::string>::const_iterator it = srv.errorPages.begin();
		for (; it != srv.errorPages.end(); ++it)
		{
			if (it->first < 300 || it->first > 599)
				throw std::runtime_error("config error: error_page code must be between 300 and 599");
		}

		// 4. Validate Locations
		for (std::size_t j = 0; j < srv.locations.size(); ++j)
		{
			const LocationConfig& loc = srv.locations[j];

			// Validate allowed methods
			for (std::size_t k = 0; k < loc.allowMethods.size(); ++k) {
				const std::string& method = loc.allowMethods[k];
				if (method != "GET" && method != "POST" && method != "DELETE")
					throw std::runtime_error("config error: invalid allow_method '" + method + "'");
			}

			// Validate redirection codes
			if (loc.hasRedirect) {
				if (loc.redirectCode != 301 && loc.redirectCode != 302 && 
					loc.redirectCode != 303 && loc.redirectCode != 307 && loc.redirectCode != 308)
					throw std::runtime_error("config error: invalid redirect code");
			}
		}
	}
}