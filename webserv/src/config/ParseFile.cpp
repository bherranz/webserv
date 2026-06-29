#include "ConfigParser.hpp"
#include "Config.hpp"

/*
** Quita todo lo que venga después de '#'.
** Caso límite: línea vacía o línea que solo es comentario.
*/
std::string ConfigParser::removeComment(const std::string& line) const
{
    std::size_t pos = line.find('#');
    if (pos == std::string::npos)
        return line;
    return line.substr(0, pos);
}

/*
** Quita espacios, tabs, '\r' y similares al principio y al final.
** Importante para soportar CRLF y config con sangrado.
*/
std::string ConfigParser::trim(const std::string& line) const
{
    std::size_t start = 0;
    std::size_t end = line.size();

    while (start < end && std::isspace(static_cast<unsigned char>(line[start])))
        ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1])))
        --end;

    return line.substr(start, end - start);
}

/*
** Convierte una línea en tokens:
** - separa por espacios
** - separa '{', '}' y ';' aunque estén pegados a una palabra
**
** Ejemplos:
**   "listen 8080;"      -> ["listen", "8080", ";"]
**   "location / {"      -> ["location", "/", "{"]
**   "server{"           -> ["server", "{"]
**
** !!!!!!!!!!!!!!!!!!!! No soporta strings con comillas por ahora!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

std::vector<std::string> ConfigParser::tokenize(const std::string& line) const
{
    std::vector<std::string> tokens;
    std::string current;

    for (std::size_t i = 0; i < line.size(); ++i)
    {
        char ch = line[i];

        if (std::isspace(static_cast<unsigned char>(ch)))
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        if (ch == '{' || ch == '}' || ch == ';')
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(1, ch));
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty())
        tokens.push_back(current);

    return tokens;
}


std::vector<ServerConfig> ConfigParser::execute(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		throw (std::runtime_error("config error: cannot open file '" + path + "'"));

	//As we can have multiple servers, we create the vector to store al of them with its own data
	std::vector<ServerConfig> servers;
	std::vector<std::string> lines;
	std::string line;

	while (std::getline(file, line))
	{
		// we clean and store
		line = removeComment(line);
		line = trim(line);
		if (!line.empty())
			lines.push_back(line); //As we used vectors, we simply perform a push_back operation to add new data
		std::cout << "LINE: [" << line << "]" << std::endl; // to be deleted
	}
	if (lines.empty())
		throw (std::runtime_error("config error: empty configuration file"));

	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		//we separate and clean the lines we detected and tokenize it so we can saved them separately
		std::vector<std::string> tokens = tokenize(lines[i]);
		if (tokens.empty())
			continue;

		//print to be deleted
		std::cout << "TOKENS: ";
		for (std::size_t j = 0; j < tokens.size(); j++)
			std::cout << "[" << tokens[j] << "] ";
		std::cout << std::endl;

		if(isServerStart(tokens)) //as we can miss a key, we check wether it is at the start of the config
			parseServerBlock(lines, i, servers);
		else
			throw (std::runtime_error("config error: expected 'server {' at top"));

	}
	return (servers);
}

std::vector<ServerConfig> ConfigParser::parseFile(const std::string& path)
{
	//Here we create the class for the parsing as in COnfigParser.hpp , all the parsed data goes there
    ConfigParser parser; 
    return parser.execute(path);
}

void Config::load(const std::string &path)
{
	_servers = ConfigParser::parseFile(path);
	print();
}