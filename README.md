# webserv
Te voy a explicar **una estructura típica de Webserv que pasa las evaluaciones de 42**, pensada para:

* C++98
* proyecto grande en equipo
* fácil de explicar en la defensa
* modular

Además te la organizo **por archivos y responsabilidades**, que es lo que te preguntan siempre en la evaluación.

---

# 1. Estructura general del proyecto

Una estructura muy usada y limpia es:

```
webserv/
│
├── Makefile
├── webserv
├── README.md
│
├── config/
│   └── default.conf
│
├── includes/
│   ├── Server.hpp
│   ├── Client.hpp
│   ├── Config.hpp
│   ├── ConfigParser.hpp
│   ├── HttpRequest.hpp
│   ├── HttpResponse.hpp
│   ├── Router.hpp
│   ├── CGIHandler.hpp
│   └── Utils.hpp
│
├── src/
│   ├── main.cpp
│   │
│   ├── server/
│   │   ├── Server.cpp
│   │   └── Client.cpp
│   │
│   ├── config/
│   │   ├── Config.cpp
│   │   └── ConfigParser.cpp
│   │
│   ├── http/
│   │   ├── HttpRequest.cpp
│   │   ├── HttpResponse.cpp
│   │   └── Router.cpp
│   │
│   ├── cgi/
│   │   └── CGIHandler.cpp
│   │
│   └── utils/
│       └── Utils.cpp
│
└── www/
    ├── index.html
    ├── error/
    │   ├── 404.html
    │   └── 500.html
    └── uploads/
```

Esta estructura separa claramente:

```
network
config
http
cgi
utils
```

Esto **queda muy bien en defensa** porque muestra diseño modular.

---

# 2. main.cpp

Este archivo solo debe **inicializar todo**.

Responsabilidades:

```
1 leer argumentos
2 cargar config
3 crear servidor
4 iniciar loop
```

Ejemplo conceptual:

```cpp
int main(int argc, char **argv)
{
    ConfigParser parser;
    Config config = parser.parse(argv[1]);

    Server server(config);
    server.start();
}
```

Importante:
En 42 valoran que **main sea muy pequeño**.

---

# 3. Server

Archivos:

```
Server.hpp
Server.cpp
```

Responsabilidad:

Gestionar **todo el servidor**.

Contiene:

```
socket
bind
listen
poll
accept
manejo de clientes
```

Variables típicas:

```cpp
int server_socket;
std::vector<pollfd> fds;
std::map<int, Client> clients;
```

Funciones importantes:

```
initSocket()
start()
runLoop()
acceptClient()
handleClient()
```

El loop principal suele ser:

```
while (true)
{
    poll()

    if (new connection)
        accept()

    if (client data)
        handle request
}
```

---

# 4. Client

Archivos:

```
Client.hpp
Client.cpp
```

Representa **un cliente conectado**.

Cada cliente tiene:

```
socket
request buffer
response buffer
estado
```

Ejemplo:

```cpp
class Client
{
    int socket;
    std::string requestBuffer;
    std::string responseBuffer;
};
```

Responsabilidades:

```
guardar request
guardar response
estado del cliente
```

Esto evita mezclar lógica en `Server`.

---

# 5. ConfigParser

Archivos:

```
ConfigParser.hpp
ConfigParser.cpp
```

Responsabilidad:

Leer el archivo:

```
default.conf
```

y convertirlo en estructuras C++.

Ejemplo de config:

```
server {
    listen 8080;
    root ./www;

    location /images {
        root ./assets;
    }
}
```

El parser debe:

```
leer lineas
tokenizar
crear estructuras
```

Salida típica:

```cpp
Config
 └── vector<ServerConfig>
```

---

# 6. Config

Archivos:

```
Config.hpp
Config.cpp
```

Define las estructuras de configuración.

Ejemplo:

```cpp
struct Location
{
    std::string path;
    std::string root;
    std::vector<std::string> methods;
};

struct ServerConfig
{
    int port;
    std::string root;
    std::vector<Location> locations;
};
```

Esto se usa luego para **decidir cómo responder**.

---

# 7. HttpRequest

Archivos:

```
HttpRequest.hpp
HttpRequest.cpp
```

Responsabilidad:

Parsear una petición HTTP.

Ejemplo de request:

```
GET /index.html HTTP/1.1
Host: localhost
Content-Length: 10
```

Debe extraer:

```
method
path
version
headers
body
```

Clase ejemplo:

```cpp
class HttpRequest
{
public:
    std::string method;
    std::string path;
    std::map<std::string,std::string> headers;
    std::string body;

    void parse(std::string raw);
};
```

---

# 8. HttpResponse

Archivos:

```
HttpResponse.hpp
HttpResponse.cpp
```

Responsabilidad:

Construir respuestas HTTP.

Ejemplo de output:

```
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 20

<html>Hello</html>
```

Clase ejemplo:

```cpp
class HttpResponse
{
public:
    int statusCode;
    std::string body;
    std::map<std::string,std::string> headers;

    std::string build();
};
```

---

# 9. Router

Archivos:

```
Router.hpp
Router.cpp
```

Responsabilidad:

Decidir **qué hacer con una request**.

Ejemplo:

```
GET /index.html
```

Router decide:

```
buscar archivo
ejecutar CGI
error
redirect
```

Flujo:

```
request
   ↓
router
   ↓
response
```

Funciones:

```
handleGet()
handlePost()
handleDelete()
```

---

# 10. CGIHandler

Archivos:

```
CGIHandler.hpp
CGIHandler.cpp
```

Responsabilidad:

Ejecutar scripts CGI.

Ejemplo:

```
GET /script.py
```

Servidor hace:

```
fork()
execve("python script.py")
pipe()
```

Debe:

```
enviar request al CGI
leer output
devolverlo al cliente
```

Esto es una de las partes **más difíciles del proyecto**.

---

# 11. Utils

Archivos:

```
Utils.hpp
Utils.cpp
```

Funciones útiles:

```
string split
trim
int to string
file exists
mime type
```

Ejemplo:

```cpp
std::vector<std::string> split(std::string str, char delimiter);
```

Esto evita duplicar código.

---

# 12. Carpeta www

Aquí van los archivos que el servidor sirve.

Ejemplo:

```
www/
  index.html
  images/
  uploads/
  error/
```

Tu servidor leerá archivos desde aquí.

---

# 13. Flujo completo del programa

Cuando alguien abre:

```
http://localhost:8080/index.html
```

ocurre esto:

```
main
 ↓
Server.start()

poll()

cliente conecta
 ↓
accept()

cliente manda request
 ↓
HttpRequest.parse()

Router

 ↓
si archivo
     leer archivo

si CGI
     ejecutar CGI

 ↓
HttpResponse.build()

 ↓
send()
```

---

# 14. Cómo dividirlo entre 3 personas

Una división muy equilibrada:

### Persona 1 — Networking

Archivos:

```
Server
Client
poll loop
socket
```

---

### Persona 2 — HTTP

Archivos:

```
HttpRequest
HttpResponse
Router
```

---

### Persona 3 — Config + CGI

Archivos:

```
Config
ConfigParser
CGIHandler
```

---

# 15. Algo que los evaluadores miran mucho

Durante la defensa preguntan:

**¿por qué separaste estas clases?**

Respuesta correcta:

```
Server → networking
HttpRequest → parsing
HttpResponse → building responses
Router → decidir comportamiento
ConfigParser → leer config
CGIHandler → ejecutar scripts
