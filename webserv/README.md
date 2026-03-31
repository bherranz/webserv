TASKS DE JAIME (1)

X Crear estructura base real:
X includes/ con Server.hpp, Client.hpp, Config.hpp, HttpRequest.hpp, HttpResponse.hpp, Router.hpp, Utils.hpp, CgiHandler.hpp
X src/main.cpp mínimo
X Ajustar Makefile para compilar rutas reales (src/.../*.cpp)
Entregar un MVP 0 en 1 día:
X socket + bind + listen en un puerto fijo
X poll() en bucle no bloqueante
accept() de clientes
al recibir datos, responder algo fijo tipo HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK
Definir interfaces compartidas (clave para no bloquearos):
firma de HttpRequest::parse(raw)
firma de Router::route(request, config) -> HttpResponse
estructuras ServerConfig y LocationConfig
Con eso, tus compañeros ya pueden desarrollar parser HTTP y config sin esperar al servidor completo.