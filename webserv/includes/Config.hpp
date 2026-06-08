#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>


//Clase base con todos los datos ncesarios para el parseo y guardado de los casos minimos segun subject
/*
load(path)
↓
abrir archivo
↓
leer línea
↓
limpiar comentario/espacios
↓
si "server {" -> crear Server
↓
si directiva del server -> setX()
↓
si "location /foo {" -> crear Location
↓
si directiva del location -> setX()
↓
si "}" de location -> addLocation()
↓
si "}" de server -> push_back(server)
↓
fin
*/

struct listen;
struct location;
struct server;

class config
{
	public:
	config();
	~config();

	/*The following structs were made as a form of organize and centralize the data according to their block.
	If we want, we have to put setters and getters to modify, even tough it would be easier in the parser
	*/
	struct listen{};
	struct location{};
	struct server{};

	void	parseConfig();
	//set y getters de <server>, ver
    std::vector<server>& servers();
    const std::vector<server>& servers() const;

	private:
	std::vector<server> _servers; //as we can have multiple servers in one config, we save them
};

struct listen
{
	// We only save the host nbr in a string for parsing, and the int value of the port
	std::string	_host;
	int			_port;
};

struct location 
{
	//here we save the data of concrete paths as location/uploads/..
	std::size_t	_clientMaxBodySize;
	std::string	_path; //prefix url as /uploads, /cgi, /errors
	std::string	_root; // path for to the folder where the files are taken of
	std::vector<std::string> _index; // name of the file/index of the file where we take information from
	std::vector<std::string> _methods; // accordign to the configuration we save multiple strings representing allowed methods

	//ver si las siguientes variables sirven asi
	std::string _redirectTarget; // url or path of redirection
	int	_redirectionCode; //http code saved as number as 404, 302, etc

	bool _autoindex;
	std::string	_uploadTo; //if we have a post method we save the path of where we save the uploaded file

	std::vector<std::string> _cgiExt; // we can have multiple extensions CGI, like .py, .php, .sh
	std::vector<std::string> _cgiPath;// we can have multiple intérpretes or associated commands CGI

	//falg block for checking while parsing
	bool _hasRoot;
	bool _hasIndex;
	bool _hasAllowMethods;
	bool _hasRedirect;
	bool _hasClientMaxBodySize;
	bool _hasAutoindex;
	bool _hasUploadTo;
	bool _hasCgi;
};

struct server
{
	//We save the configuration data of a server as a block, as we can have multiple
	std::string	_host;
	std::vector<listen> _listens; //as we can listen to multiple endpoints
	std::string	_serverName;
	std::string _root;
	std::string	_index;
	std::map<int, std::string>	_errorPages;
	std::size_t	_clientMaxBodySize;

	std::vector<location> _locations;//as we are allowed to have multiple locations paths
	
	//again for checking
	bool _hasHost;
	bool _hasRoot;
	bool _hasIndex;
	bool _hasClientMaxBodySize;
};