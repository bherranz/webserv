#include "ConfigParser.hpp"
#include "Config.hpp"

std::string ConfigParser::removeComment(const std::string& line) const
{
    std::size_t pos = line.find('#');
    if (pos == std::string::npos)
        return line;
    return line.substr(0, pos);
}


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
	std::vector<ServerConfig> servers;
	std::vector<std::vector<std::string> > statements;
	std::vector<std::string> currentStatement;
	std::string line;

	// FASE 1: Group tokens in instrucctions
	while (std::getline(file, line))
	{
		line = removeComment(line);
		line = trim(line);
		if (line.empty()) continue;

		std::vector<std::string> lineTokens = tokenize(line);

		for (std::size_t j = 0; j < lineTokens.size(); j++)
		{
			currentStatement.push_back(lineTokens[j]);

			// If we find a delimitator, end and save instrucction
			if (lineTokens[j] == "{" || lineTokens[j] == "}" || lineTokens[j] == ";")
			{
				statements.push_back(currentStatement);
				currentStatement.clear();
			}
		}
	}
	if (!currentStatement.empty())
		statements.push_back(currentStatement);

	if (statements.empty())
		throw (std::runtime_error("config error: empty configuration file"));

	// FASE 2: Iterate and pass blocks to parse
	for (std::size_t i = 0; i < statements.size(); ++i)
	{
		const std::vector<std::string>& tokens = statements[i];

		if (tokens.empty()) continue;

		if (isServerStart(tokens))
			parseServerBlock(statements, i, servers);
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
	validate();
	print();
}
