#include "ConfigParser.hpp"

void ConfigParser::validateSemicolon(const std::vector<std::string>& tokens) const
{
	if (tokens.size() < 2 || tokens.back() != ";")
		throw std::runtime_error("config error: directive must end with ';'");	
}

// ==========================================================================
// Templates block
// ==========================================================================

template <typename T>
void ConfigParser::parseIndex(const std::vector<std::string>& tokens, T& config)
{
	validateSemicolon(tokens);
	for (size_t i = 1; i < tokens.size() - 1; i++) //we add all of the root info as we already checked the ;
		config.index.push_back(tokens[i]);
	config.hasIndex = true; //changing the state
}

template <typename T>
void ConfigParser::parseRoot(const std::vector<std::string>& tokens, T& config)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid number of arguments in 'root'");
	config.root = tokens[1];
	config.hasRoot = true;
}

template <typename T>
void ConfigParser::parseClientMaxBodySize(const std::vector<std::string>& tokens, T& config)
{
	validateSemicolon(tokens);
	if (tokens.size() != 3)
		throw std::runtime_error("config error: invalid number of arguments in 'client_maxBody_size'");
	long long size = std::atol(tokens[1].c_str());
    if (size < 0)
        throw std::runtime_error("config error: client_max_body_size cannot be negative");
    
    config.clientMaxBodySize = static_cast<std::size_t>(size);
	config.hasClientMaxBodySize = true;
}

// ==========================================================================
// Maps innit
// ==========================================================================

void ConfigParser::initDirectiveParsers()
{
	// --- SERVER MAP ---
	_serverParsers["listen"] = &ConfigParser::parseListen;
	_serverParsers["server_name"] = &ConfigParser::parseServerName;
	_serverParsers["error_page"] = &ConfigParser::parseErrorPage;
	_serverParsers["host"] = &ConfigParser::parseHost; //->> NGNIX DOEST SAVE THE HOST INDEPENDANTLY, IT ALWAYS GOES IN THE LISTEN BLOCK -- CHECK IF DELETE
    _serverParsers["client_timeout"] = &ConfigParser::parseClientTimeout;
    _serverParsers["keepalive_timeout"] = &ConfigParser::parseKeepaliveTimeout;
	_serverParsers["autoindex"] = &ConfigParser::parseAutoindex<ServerConfig>; // <--- Añadir

	// The following use the templates using ServerConfig
	_serverParsers["root"] = &ConfigParser::parseRoot<ServerConfig>;
	_serverParsers["index"] = &ConfigParser::parseIndex<ServerConfig>;
	_serverParsers["client_max_body_size"] = &ConfigParser::parseClientMaxBodySize<ServerConfig>;

	// --- LOCATION MAP ---
	_locationParsers["allow_methods"] = &ConfigParser::parseAllowMethods;
	_locationParsers["autoindex"] = &ConfigParser::parseAutoindex<LocationConfig>;
	_locationParsers["upload_store"] = &ConfigParser::parseUploadStore;
	_locationParsers["cgi_path"] = &ConfigParser::parseCgiPath;
	_locationParsers["cgi_ext"] = &ConfigParser::parseCgiExt;
	_locationParsers["return"] = &ConfigParser::parseReturn;

	// The following use the templates using LocationConfig
	_locationParsers["root"] = &ConfigParser::parseRoot<LocationConfig>;
	_locationParsers["index"] = &ConfigParser::parseIndex<LocationConfig>;
	_locationParsers["client_max_body_size"] = &ConfigParser::parseClientMaxBodySize<LocationConfig>;
}