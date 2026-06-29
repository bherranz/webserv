#pragma once

#include "Config.hpp"

class ConfigParser
{
	public:
	ConfigParser();
	static std::vector<ServerConfig> parseFile(const std::string& path);

	private:
	std::vector<ServerConfig> execute(const std::string& path);
	/* ==========================   PARSE FILE   ========================== */


    std::string removeComment(const std::string& line) const;
    std::string trim(const std::string& line) const;
    std::vector<std::string> tokenize(const std::string& line) const;

	/* ==========================   PARSE BLOCK   ========================== */

    bool isServerStart(const std::vector<std::string>& tokens) const;
    bool isBlockEnd(const std::vector<std::string>& tokens) const;
    bool isLocationStart(const std::vector<std::string>& tokens) const;

	void parseServerBlock(const std::vector<std::string>& lines, std::size_t& i, std::vector<ServerConfig>& servers);
    void parseLocationBlock(const std::vector<std::string>& lines, std::size_t& i, ServerConfig& server);
 

	/* ==========================   PARSE DIRECTIVES   ========================== */
	/*-----------------------   Directive maps -------------------------------------*/
	typedef void (ConfigParser::*ServerDirectiveFn)(const std::vector<std::string>&, ServerConfig&);
	typedef void (ConfigParser::*LocationDirectiveFn)(const std::vector<std::string>&, LocationConfig&);

	std::map<std::string, ServerDirectiveFn> _serverParsers;
	std::map<std::string, LocationDirectiveFn> _locationParsers;

	void initDirectiveParsers();
	void validateSemicolon(const std::vector<std::string>& tokens) const;

	/*-----------------------  Shared directives (Templates) -------------------------------------*/
	//As Location and Server have these same data sitaxis we recycle code
	template <typename T> // -> Template def for the following method
    void parseRoot(const std::vector<std::string>& tokens, T& config); // T& can be variable, hence we can pass it the location or server struct
    
    template <typename T>
    void parseIndex(const std::vector<std::string>& tokens, T& config);
    
    template <typename T>
    void parseClientMaxBodySize(const std::vector<std::string>& tokens, T& config);

	/*-----------------------  Location directives 			 -------------------------------------*/
	void parseAllowMethods(const std::vector<std::string>& tokens, LocationConfig& location);
    void parseAutoindex(const std::vector<std::string>& tokens, LocationConfig& location);
    void parseUploadStore(const std::vector<std::string>& tokens, LocationConfig& location);
    void parseCgiPath(const std::vector<std::string>& tokens, LocationConfig& location);
    void parseCgiExt(const std::vector<std::string>& tokens, LocationConfig& location);
    void parseReturn(const std::vector<std::string>& tokens, LocationConfig& location);

	/*-----------------------  Server directives 			 -------------------------------------*/
	void parseListen(const std::vector<std::string>& tokens, ServerConfig& server);
    void parseServerName(const std::vector<std::string>& tokens, ServerConfig& server);
    void parseErrorPage(const std::vector<std::string>& tokens, ServerConfig& server);
	void parseClientTimeout(const std::vector<std::string>& tokens, ServerConfig& server);
	void parseKeepaliveTimeout(const std::vector<std::string>& tokens, ServerConfig& server);
	//->> NGNIX DOEST SAVE THE HOST INDEPENDANTLY, IT ALWAYS GOES IN THE LISTEN BLOCK -- CHECK IF DELETE
	void parseHost(const std::vector<std::string>& tokens, ServerConfig& server);
};
