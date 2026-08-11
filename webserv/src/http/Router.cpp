#include "Router.hpp"
#include "Utils.hpp"
#include "Config.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cerrno>

Router::Router() {}
Router::~Router() {}

const LocationConfig *Router::matchLocation(const std::string &path, const ServerConfig &server) const
{
	const LocationConfig *best = NULL;
	std::size_t bestLen = 0;

	for (std::size_t i = 0; i < server.locations.size(); ++i)
	{
		const LocationConfig &loc = server.locations[i];
		if (path.compare(0, loc.path.size(), loc.path) == 0)
		{
			if (loc.path.size() > bestLen)
			{
				if (loc.path.size() == 1 && loc.path[0] == '/')
				{
					best = &loc;
					bestLen = 1;
					continue;
				}
				if (path.size() == loc.path.size() || path[loc.path.size()] == '/' || path[loc.path.size()] == '?')
				{
					best = &loc;
					bestLen = loc.path.size();
				}
			}
		}
	}
	return best;
}

bool Router::isMethodAllowed(const std::string &method, const LocationConfig &loc) const
{
	if (!loc.hasAllowMethods)
		return true;
	for (std::size_t i = 0; i < loc.allowMethods.size(); ++i)
	{
		if (loc.allowMethods[i] == method)
			return true;
	}
	return false;
}

std::string Router::resolvePath(const std::string &uriPath, const LocationConfig &loc, const ServerConfig &server) const
{
	std::string root = ".";
	if (loc.hasRoot && !loc.root.empty())
		root = loc.root;
	else if (server.hasRoot && !server.root.empty())
		root = server.root;

	std::string cleanPath = Utils::urlDecode(Utils::stripQueryString(uriPath));
	return Utils::joinPath(root, cleanPath);
}
	
std::string Router::resolveIndex(const std::string &dirPath, const LocationConfig &loc, const ServerConfig &server) const
{
	const std::vector<std::string> *indexList = NULL;
	std::vector<std::string> fallback;
	if (loc.hasIndex && !loc.index.empty())
		indexList = &loc.index;
	else if (server.hasIndex && !server.index.empty())
		indexList = &server.index;
	else
	{
		fallback.push_back("index.html");
		indexList = &fallback;
	}

	for (std::size_t i = 0; i < indexList->size(); ++i)
	{
		std::string indexPath = Utils::joinPath(dirPath, (*indexList)[i]);

		if (Utils::fileExists(indexPath))
			return indexPath;
	}

	return "";
}

CgiFds Router::route(const HttpRequest &req, HttpResponse &res, const ServerConfig &server)
{
	CgiFds noCgi = {-1, -1, -1};

	res.clear();

	const LocationConfig *loc = matchLocation(req._path, server);

	if (loc != NULL && loc->hasRedirect && !loc->redirectTarget.empty())
	{
		handleRedirect(req, res, *loc);
		return noCgi;
	}
	if (loc != NULL && !isMethodAllowed(req._method, *loc))
	{
		buildErrorResponse(res, 405, server);
		std::string allow;
		for (std::size_t i = 0; i < loc->allowMethods.size(); ++i)
		{
			if (i > 0)
				allow += ", ";
			allow += loc->allowMethods[i];
		}
		res.setHeader("Allow", allow);
		return noCgi;
	}
	else if (server.hasClientMaxBodySize && server.clientMaxBodySize > 0)
	{
		if (req._body.size() > server.clientMaxBodySize)
		{
			buildErrorResponse(res, 413, server);
			return noCgi;
		}
	}
	// If it is a CGI request, handle it and return the fds
	if (loc != NULL && isCgiExtension(req._path, *loc))
		return handleCgi(req, res, *loc, server);

	if (req._method == "GET")
		handleGet(req, res, loc ? *loc : LocationConfig(), server);
	else if (req._method == "POST")
		handlePost(req, res, loc ? *loc : LocationConfig(), server);
	else if (req._method == "DELETE")
		handleDelete(req, res, loc ? *loc : LocationConfig(), server);
	else
		buildErrorResponse(res, 501, server);

	return noCgi;
}

void Router::handleGet(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server)
{
	std::string fsPath = resolvePath(req._path, loc, server);

	// 1. Si es un Directorio
	if (Utils::isDirectory(fsPath))
	{
		std::string indexPath = resolveIndex(fsPath, loc, server);
		if (!indexPath.empty())
		{
			std::string content = Utils::readFile(indexPath);
			if (content.empty())
			{
				buildErrorResponse(res, 404, server);
				return;
			}
			std::size_t dot = indexPath.rfind('.');
			std::string ext = (dot != std::string::npos) ? indexPath.substr(dot) : "";
			res.setStatus(200, "OK");
			res.setContentType(Utils::getMimeType(ext));
			res.setBody(content);
			res.setContentLength(content.size());
			return;
		}
		else
		{
			bool isAutoindex = false;
			if (loc.hasAutoindex) 
				isAutoindex = loc.autoindex; // Prioridad 1: Lo que diga la Location
			else if (server.hasAutoindex) 
				isAutoindex = server.autoindex; // Prioridad 2: Lo que diga el Server

			if (isAutoindex)
			{
				handleAutoindex(fsPath, res);
				return;
			}
			
			buildErrorResponse(res, 403, server);
			return;
		}
	}

	if (Utils::fileExists(fsPath))
	{
		std::string content = Utils::readFile(fsPath);
		if (content.empty())
		{
			buildErrorResponse(res, 404, server);
			return;
		}
		std::size_t dot = fsPath.rfind('.');
		std::string ext = (dot != std::string::npos) ? fsPath.substr(dot) : "";
		res.setStatus(200, "OK");
		res.setContentType(Utils::getMimeType(ext));
		res.setBody(content);
		res.setContentLength(content.size());
		return;
	}

	buildErrorResponse(res, 404, server);
}


void Router::handlePost(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server)
{
	// Check for multipart/form-data upload
	std::map<std::string, std::string>::const_iterator ct = req._headers.find("content-type");
	if (ct != req._headers.end() && ct->second.find("multipart/form-data") != std::string::npos)
	{
		std::string boundary;
		std::size_t bpos = ct->second.find("boundary=");
		if (bpos != std::string::npos)
			boundary = ct->second.substr(bpos + 9);

		if (!boundary.empty())
		{
			std::string filename;
			std::string content;
			if (parseMultipart(req._body, boundary, filename, content))
			{
				if (!content.empty())
				{
					std::string root = ".";
					if (loc.hasUploadStore && !loc.uploadStore.empty())
						root = loc.uploadStore;
					else if (loc.hasRoot && !loc.root.empty())
						root = loc.root;
					else if (server.hasRoot && !server.root.empty())
						root = server.root;

					// Sanitize filename: remove path separators
					std::string safeName = filename;
					std::size_t sep = safeName.rfind('/');
					if (sep != std::string::npos)
						safeName = safeName.substr(sep + 1);
					sep = safeName.rfind('\\');
					if (sep != std::string::npos)
						safeName = safeName.substr(sep + 1);

					std::string fsPath = Utils::joinPath(root, safeName);

					std::ofstream file(fsPath.c_str(), std::ios::binary);
					if (file.is_open())
					{
						file.write(content.c_str(), content.size());
						file.close();
						res.setStatus(201, "Created");
						res.setBody("");
						res.setContentLength(0);
						return;
					}
				}
			}
		}
		// Fall through to normal POST on parse failure
	}

	std::string root = ".";
	if (loc.hasUploadStore && !loc.uploadStore.empty())
		root = loc.uploadStore;
	else if (loc.hasRoot && !loc.root.empty())
		root = loc.root;
	else if (server.hasRoot && !server.root.empty())
		root = server.root;

	std::string cleanPath = Utils::urlDecode(Utils::stripQueryString(req._path));

	// Strip the location prefix from the URI so uploaded files land in upload_store
	// without duplicating the location path segment.
	if (loc.hasUploadStore && !loc.uploadStore.empty() && !loc.path.empty())
	{
		if (cleanPath.compare(0, loc.path.size(), loc.path) == 0)
			cleanPath = cleanPath.substr(loc.path.size());
	}

	std::string fsPath = Utils::joinPath(root, cleanPath);

	if (Utils::isDirectory(fsPath))
	{
		buildErrorResponse(res, 403, server);
		return;
	}

	std::string dirPath = fsPath;
	std::size_t slash = fsPath.rfind('/');
	if (slash != std::string::npos)
		dirPath = fsPath.substr(0, slash);

	if (!Utils::fileExists(dirPath))
	{
		buildErrorResponse(res, 404, server);
		return;
	}

	if (req._body.empty())
	{
		buildErrorResponse(res, 400, server);
		return;
	}

	std::ofstream file(fsPath.c_str(), std::ios::binary);
	if (!file.is_open())
	{
		buildErrorResponse(res, 403, server);
		return;
	}
	file.write(req._body.c_str(), req._body.size());
	file.close();

	res.setStatus(201, "Created");
	res.setBody("");
	res.setContentLength(0);
}

void Router::handleDelete(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server)
{
	std::string fsPath = resolvePath(req._path, loc, server);

	if (Utils::isDirectory(fsPath))
	{
		buildErrorResponse(res, 403, server);
		return;
	}

	if (!Utils::fileExists(fsPath))
	{
		buildErrorResponse(res, 404, server);
		return;
	}

	if (::unlink(fsPath.c_str()) != 0)
	{
		buildErrorResponse(res, 403, server);
		return;
	}

	res.setStatus(204, "No Content");
	res.setBody("");
	res.setContentLength(0);
}

void Router::handleRedirect(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc)
{
	(void)req;
	int code = loc.redirectCode;
	if (code == 0)
		code = 301;

	res.setStatus(code, HttpResponse::reasonPhrase(code));
	res.setHeader("Location", loc.redirectTarget);
	res.setBody("");
	res.setContentLength(0);
}

CgiFds Router::handleCgi(const HttpRequest &req, HttpResponse &res, const LocationConfig &loc, const ServerConfig &server)
{
	CGIHandler handler(req, loc, server);
	// Start the CGI process and get the file descriptors
	return handler.start(res); 
}

void Router::handleAutoindex(const std::string &dirPath, HttpResponse &res)
{
	DIR *dir = opendir(dirPath.c_str());
	if (dir == NULL)
	{
		res.setError(403);
		return;
	}

	std::ostringstream html;
	html << "<html><head><title>Index of " << dirPath << "</title></head><body>";
	html << "<h1>Index of " << dirPath << "</h1><hr><pre>";

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name(entry->d_name);
		if (name == ".")
			continue;

		std::string link = name;
		if (entry->d_type == DT_DIR)
			link += "/";

		html << "<a href=\"" << name;
		if (entry->d_type == DT_DIR)
			html << "/";
		html << "\">" << link << "</a>\n";
	}
	closedir(dir);

	html << "</pre><hr></body></html>";

	std::string body = html.str();
	res.setStatus(200, "OK");
	res.setContentType("text/html");
	res.setBody(body);
	res.setContentLength(body.size());
}

void Router::buildErrorResponse(HttpResponse &res, int code, const ServerConfig &server)
{
	std::map<int, std::string>::const_iterator it = server.errorPages.find(code);
	if (it != server.errorPages.end())
	{
		std::string root = (server.hasRoot && !server.root.empty()) ? server.root : ".";
		std::string fullPath = Utils::joinPath(root, it->second);
		std::string content = Utils::readFile(fullPath);
		if (!content.empty())
		{
			res.setStatus(code, HttpResponse::reasonPhrase(code));
			res.setContentType("text/html");
			res.setBody(content);
			res.setContentLength(content.size());
			return;
		}
	}

	res.setError(code);
}

bool Router::parseMultipart(const std::string &body, const std::string &boundary,
	std::string &outFilename, std::string &outContent)
{
	std::string delimiter = "--" + boundary;

	std::size_t pos = 0;
	while (true)
	{
		std::size_t partStart = body.find(delimiter, pos);
		if (partStart == std::string::npos)
			break;

		std::size_t headerStart = partStart + delimiter.size();

		if (body.compare(headerStart, 2, "--") == 0)
			break;

		if (body.compare(headerStart, 2, "\r\n") == 0)
			headerStart += 2;
		else if (body[headerStart] == '\n')
			headerStart += 1;

		std::size_t headerEnd = body.find("\r\n\r\n", headerStart);
		if (headerEnd == std::string::npos)
		{
			headerEnd = body.find("\n\n", headerStart);
			if (headerEnd == std::string::npos)
				break;
			std::size_t contentEnd = body.find(delimiter, headerEnd + 2);
			if (contentEnd == std::string::npos)
				break;
			std::string partHeaders = body.substr(headerStart, headerEnd - headerStart);
			std::string content = body.substr(headerEnd + 2, contentEnd - headerEnd - 2);
			if (!content.empty() && content[content.size() - 1] == '\n')
				content.erase(content.size() - 1);
			if (!content.empty() && content[content.size() - 1] == '\r')
				content.erase(content.size() - 1);
			if (partHeaders.find("filename=") != std::string::npos)
			{
				std::size_t fnStart = partHeaders.find("filename=") + 9;
				if (fnStart < partHeaders.size() && (partHeaders[fnStart] == '"' || partHeaders[fnStart] == '\''))
					fnStart += 1;
				std::size_t fnEnd = partHeaders.find_first_of("\"'\r\n;", fnStart);
				if (fnEnd == std::string::npos)
					fnEnd = partHeaders.size();
				outFilename = partHeaders.substr(fnStart, fnEnd - fnStart);
				outContent = content;
				return true;
			}
			break;
		}

		std::string partHeaders = body.substr(headerStart, headerEnd - headerStart);
		std::size_t contentStart = headerEnd + 4;
		std::size_t contentEnd = body.find(delimiter, contentStart);
		if (contentEnd == std::string::npos)
			break;
		std::string content = body.substr(contentStart, contentEnd - contentStart);
		if (!content.empty() && content[content.size() - 1] == '\n')
			content.erase(content.size() - 1);
		if (!content.empty() && content[content.size() - 1] == '\r')
			content.erase(content.size() - 1);

		if (partHeaders.find("filename=") != std::string::npos)
		{
			std::size_t fnStart = partHeaders.find("filename=") + 9;
			if (fnStart < partHeaders.size() && (partHeaders[fnStart] == '"' || partHeaders[fnStart] == '\''))
				fnStart += 1;
			std::size_t fnEnd = partHeaders.find_first_of("\"'\r\n;", fnStart);
			if (fnEnd == std::string::npos)
				fnEnd = partHeaders.size();
			outFilename = partHeaders.substr(fnStart, fnEnd - fnStart);
			outContent = content;
			return true;
		}

		pos = contentEnd;
	}
	return false;
}
