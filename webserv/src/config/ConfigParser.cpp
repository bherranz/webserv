/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jaime <jaime@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:55:25 by miparis           #+#    #+#             */
/*   Updated: 2026/05/28 13:05:00 by jaime            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
	struct Token {
		std::string value;
		std::size_t line;
		std::size_t column;
	};

	void pushToken(std::vector<Token> &tokens, const std::string &value, std::size_t line, std::size_t column) {
		Token token;
		token.value = value;
		token.line = line;
		token.column = column;
		tokens.push_back(token);
	}

	std::string errorAt(const Token &token, const std::string &message) {
		std::ostringstream out;
		out << "config error at line " << token.line << ", column " << token.column << ": " << message;
		return out.str();
	}

	bool isNumber(const std::string &text) {
		if (text.empty())
			return false;
		for (std::size_t i = 0; i < text.size(); ++i) {
			if (!std::isdigit(static_cast<unsigned char>(text[i])))
				return false;
		}
		return true;
	}

	std::string lowerCopy(const std::string &text) {
		std::string result = text;
		for (std::size_t i = 0; i < result.size(); ++i)
			result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
		return result;
	}

	unsigned long parseUnsigned(const Token &token, const std::string &text) {
		if (!isNumber(text))
			throw std::runtime_error(errorAt(token, "invalid numeric value '" + text + "'"));
		errno = 0;
		char *end = NULL;
		unsigned long value = std::strtoul(text.c_str(), &end, 10);
		if (errno == ERANGE || end == text.c_str() || *end != '\0')
			throw std::runtime_error(errorAt(token, "numeric value out of range '" + text + "'"));
		return value;
	}

	std::vector<Token> tokenize(const std::string &content) {
		std::vector<Token> tokens;
		std::string current;
		std::size_t currentLine = 1;
		std::size_t currentColumn = 1;
		std::size_t line = 1;
		std::size_t column = 1;

		for (std::size_t i = 0; i < content.size(); ++i) {
			char ch = content[i];
			if (ch == '#') {
				if (!current.empty()) {
					pushToken(tokens, current, currentLine, currentColumn);
					current.clear();
				}
				while (i < content.size() && content[i] != '\n')
					++i;
				if (i == content.size())
					break;
				ch = content[i];
			}
			if (ch == '\n') {
				if (!current.empty()) {
					pushToken(tokens, current, currentLine, currentColumn);
					current.clear();
				}
				++line;
				column = 1;
				continue;
			}
			if (std::isspace(static_cast<unsigned char>(ch))) {
				if (!current.empty()) {
					pushToken(tokens, current, currentLine, currentColumn);
					current.clear();
				}
				++column;
				continue;
			}
			if (ch == '{' || ch == '}' || ch == ';') {
				if (!current.empty()) {
					pushToken(tokens, current, currentLine, currentColumn);
					current.clear();
				}
				pushToken(tokens, std::string(1, ch), line, column);
				++column;
				continue;
			}
			if (current.empty()) {
				currentLine = line;
				currentColumn = column;
			}
			current.push_back(ch);
			++column;
		}
		if (!current.empty())
			pushToken(tokens, current, currentLine, currentColumn);
		return tokens;
	}

	class Parser {
	public:
		explicit Parser(const std::vector<Token> &tokens) : _tokens(tokens), _index(0) {}

		Config parse() {
			Config config;
			while (!eof())
				config.servers().push_back(parseServer());
			if (config.servers().empty())
				throw std::runtime_error("config error: at least one server block is required");
			return config;
		}

	private:
		const std::vector<Token> &_tokens;
		std::size_t _index;

		bool eof() const { return _index >= _tokens.size(); }
		const Token &peek() const { return _tokens[_index]; }
		const Token &next() { return _tokens[_index++]; }

		bool match(const std::string &value) const {
			return !eof() && _tokens[_index].value == value;
		}

		const Token &expect(const std::string &value) {
			if (eof() || peek().value != value)
				throw std::runtime_error(errorAt(eof() ? _tokens.back() : peek(), "expected '" + value + "'"));
			return next();
		}

		std::string expectIdentifier() {
			if (eof())
				throw std::runtime_error("config error: unexpected end of file");
			return next().value;
		}

		int parsePort(const Token &token, const std::string &text) {
			unsigned long value = parseUnsigned(token, text);
			if (value == 0 || value > 65535)
				throw std::runtime_error(errorAt(token, "port out of range '" + text + "'"));
			return static_cast<int>(value);
		}

		std::size_t parseBodySize(const Token &token, const std::string &text) {
			unsigned long value = parseUnsigned(token, text);
			if (value > std::numeric_limits<std::size_t>::max())
				throw std::runtime_error(errorAt(token, "body size out of range '" + text + "'"));
			return static_cast<std::size_t>(value);
		}

		int parseStatusCode(const Token &token, const std::string &text) {
			unsigned long value = parseUnsigned(token, text);
			if (value < 100 || value > 599)
				throw std::runtime_error(errorAt(token, "invalid status code '" + text + "'"));
			return static_cast<int>(value);
		}

		bool isMethod(const std::string &method) const {
			return method == "GET" || method == "POST" || method == "DELETE";
		}

		bool locationPathExists(const ServerConfig &server, const std::string &path) const {
			for (std::size_t i = 0; i < server.locations.size(); ++i) {
				if (server.locations[i].path == path)
					return true;
			}
			return false;
		}

		ListenConfig parseListen(const Token &directiveToken, const ServerConfig &server) {
			if (eof())
				throw std::runtime_error(errorAt(directiveToken, "missing listen value"));
			std::string value = expectIdentifier();
			expect(";");

			std::string host;
			int port = -1;
			std::size_t separator = value.find(':');
			if (separator != std::string::npos) {
				if (separator == 0 || separator + 1 >= value.size())
					throw std::runtime_error(errorAt(directiveToken, "invalid listen directive '" + value + "'"));
				host = value.substr(0, separator);
				port = parsePort(directiveToken, value.substr(separator + 1));
			} else {
				port = parsePort(directiveToken, value);
				if (server.hasHost)
					host = server.host;
			}
			return ListenConfig(host, port);
		}

		LocationConfig parseLocation(ServerConfig &server) {
			LocationConfig location;
			location.path = expectIdentifier();
			if (locationPathExists(server, location.path))
				throw std::runtime_error(errorAt(_tokens[_index - 1], "duplicate location path '" + location.path + "'"));
			expect("{");
			while (!eof() && !match("}")) {
				Token directiveToken = next();
				std::string directive = directiveToken.value;
				if (directive == "root") {
					location.root = expectIdentifier();
					location.hasRoot = true;
					expect(";");
				} else if (directive == "index") {
					location.index.clear();
					while (!eof() && !match(";"))
						location.index.push_back(expectIdentifier());
					location.hasIndex = true;
					expect(";");
				} else if (directive == "autoindex") {
					std::string value = lowerCopy(expectIdentifier());
					if (value == "on")
						location.autoindex = true;
					else if (value == "off")
						location.autoindex = false;
					else
						throw std::runtime_error(errorAt(_tokens[_index - 1], "autoindex must be 'on' or 'off'"));
					location.hasAutoindex = true;
					expect(";");
				} else if (directive == "allow_methods") {
					location.allowMethods.clear();
					while (!eof() && !match(";")) {
						std::string method = expectIdentifier();
						if (!isMethod(method))
							throw std::runtime_error(errorAt(_tokens[_index - 1], "unsupported method '" + method + "'"));
						location.allowMethods.push_back(method);
					}
					location.hasAllowMethods = true;
					expect(";");
				} else if (directive == "return") {
					std::vector<std::string> values;
					while (!eof() && !match(";"))
						values.push_back(expectIdentifier());
					expect(";");
					if (values.empty())
						throw std::runtime_error(errorAt(directiveToken, "return directive requires a target"));
					if (values.size() == 1) {
						location.redirectTarget = values[0];
					} else if (values.size() == 2 && isNumber(values[0])) {
						location.redirectCode = parseStatusCode(directiveToken, values[0]);
						location.redirectTarget = values[1];
					} else {
						throw std::runtime_error(errorAt(directiveToken, "invalid return directive"));
					}
					location.hasRedirect = true;
				} else if (directive == "client_max_body_size") {
					location.clientMaxBodySize = parseBodySize(directiveToken, expectIdentifier());
					location.hasClientMaxBodySize = true;
					expect(";");
				} else if (directive == "cgi_path") {
					while (!eof() && !match(";"))
						location.cgiPath.push_back(expectIdentifier());
					expect(";");
				} else if (directive == "cgi_ext") {
					while (!eof() && !match(";"))
						location.cgiExt.push_back(expectIdentifier());
					expect(";");
				} else {
					throw std::runtime_error(errorAt(directiveToken, "unknown location directive '" + directive + "'"));
				}
			}
			expect("}");
			return location;
		}

		ServerConfig parseServer() {
			ServerConfig server;
			expect("server");
			expect("{");
			while (!eof() && !match("}")) {
				Token directiveToken = next();
				std::string directive = directiveToken.value;
				if (directive == "listen") {
					server.listens.push_back(parseListen(directiveToken, server));
				} else if (directive == "host") {
					server.host = expectIdentifier();
					server.hasHost = true;
					expect(";");
				} else if (directive == "server_name") {
					while (!eof() && !match(";"))
						server.serverNames.push_back(expectIdentifier());
					expect(";");
				} else if (directive == "root") {
					server.root = expectIdentifier();
					server.hasRoot = true;
					expect(";");
				} else if (directive == "index") {
					server.index.clear();
					while (!eof() && !match(";"))
						server.index.push_back(expectIdentifier());
					server.hasIndex = true;
					expect(";");
				} else if (directive == "client_max_body_size") {
					server.clientMaxBodySize = parseBodySize(directiveToken, expectIdentifier());
					server.hasClientMaxBodySize = true;
					expect(";");
				} else if (directive == "error_page") {
					std::vector<std::string> values;
					while (!eof() && !match(";"))
						values.push_back(expectIdentifier());
					expect(";");
					if (values.size() < 2)
						throw std::runtime_error(errorAt(directiveToken, "error_page requires at least one status code and a path"));
					std::string page = values.back();
					for (std::size_t i = 0; i + 1 < values.size(); ++i)
						server.errorPages[parseStatusCode(directiveToken, values[i])] = page;
				} else if (directive == "location") {
					LocationConfig location = parseLocation(server);
					server.locations.push_back(location);
				} else {
					throw std::runtime_error(errorAt(directiveToken, "unknown server directive '" + directive + "'"));
				}
			}
			expect("}");
			if (server.listens.empty())
				server.listens.push_back(ListenConfig(server.hasHost ? server.host : std::string("0.0.0.0"), 8080));
			if (!server.hasHost)
				server.host = server.listens.front().host.empty() ? std::string("0.0.0.0") : server.listens.front().host;
			if (server.host.empty())
				server.host = "0.0.0.0";
			for (std::size_t i = 0; i < server.listens.size(); ++i) {
				if (server.listens[i].host.empty())
					server.listens[i].host = server.host;
			}
			return server;
		}
	};
}

Config ConfigParser::parseFile(const std::string &path) {
	std::ifstream file(path.c_str());
	if (!file.is_open())
		throw std::runtime_error("config error: cannot open file '" + path + "'");

	std::ostringstream buffer;
	buffer << file.rdbuf();
	std::vector<Token> tokens = tokenize(buffer.str());
	if (tokens.empty())
		throw std::runtime_error("config error: empty configuration file");

	Parser parser(tokens);
	return parser.parse();
}/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/20 15:55:25 by miparis           #+#    #+#             */
/*   Updated: 2026/03/20 15:55:37 by miparis          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"