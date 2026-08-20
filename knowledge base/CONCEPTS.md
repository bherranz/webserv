# Webserv Concepts / Conceptos de Webserv

This document contains the foundational knowledge base, architecture, and flow of the Webserv project.
*Este documento contiene la base de conocimiento, arquitectura y flujo del proyecto Webserv.*

---
## How it works

* Server → networking
* HttpRequest → parsing
* HttpResponse → building responses
* Router → decidir comportamiento
* ConfigParser → leer config
* CGIHandler → ejecutar scripts


### Server Operation Diagram

                     CONFIG
                        │
                        ▼
              host = 127.0.0.1
              port = 8080
                        │
                        ▼
                    socket()
                        │
                        ▼
                     bind()
                        │
                        ▼
                    listen()
                        │
                        ▼
             ┌──────────────────────┐
             │   SERVER SOCKET      │
             │   127.0.0.1:8080     │
             └──────────┬───────────┘
                        │
                        ▼
                  ┌───────────┐
    ┌────────────►│   poll()  │◄────────────────────────────────┐
    │             └─────┬─────┘                                 │
    │                   │                                       │
    │         ┌─────────┴──────────┐                            │
    │         │                    │                            │
    │         ▼                    ▼                            │
    │  serverFd listo        clientFd listo                     │
    │         │                    │                            │
    │         ▼                    ▼                            │
    │     accept()              recv()                          │
    │         │                    │                            │
    │         ▼                    ▼                            │
    │   nuevo clientFd       HTTP Request                       │
    │         │                    │                            │
    │         └──────────┐         ▼                            │
    │                    │    parse HTTP                        │
    │                    │         │                            │
    │                    │         ▼                            │
    │                    │      route                           │
    │                    │         │                            │
    │                    │    ┌────┼────┐                       │
    │                    │    ▼    ▼    ▼                       │
    │                    │ static CGI upload/error              │
    │                    │    │    │    │                       │
    │                    │    └────┼────┘                       │
    │                    │         ▼                            │
    │                    │   HTTP Response                      │
    │                    │         │                            │
    │                    └────────►│                            │
    │                              ▼                            │
    │                         poll(POLLOUT)                     │
    │                              │                            │
    │                              ▼                            │
    │                            send()                         │
    │                              │                            │
    │                         ¿terminado?                       │
    │                          │       │                        │
    │                         NO      SÍ                        │
    │                          │       │                        │
    │                          │       ▼                        │
    │                          │  keep-alive?                   │
    │                          │    │       │                   │
    │                          │   NO      SÍ                   │
    │                          │    │       │                   │
    └──────────────────────────┘  close     └───────────────────┘


## 🇬🇧 English Version

### 1. What is the Server and how is it initialized?
A web server is fundamentally a program designed to listen for network requests, process them, and return valid responses under the HTTP protocol. To achieve this, it relies on the operating system's socket API, managed in files like `Server.cpp` and `Client.cpp`[cite: 4].

* **Sockets and File Descriptors (`server_fd` and `client_fd`)**: A File Descriptor (FD) is a number the OS uses to identify a network resource.
  * The **`server_fd`** (Server Socket) is the main listening line. It is created at startup, and its sole purpose is to listen for and accept new incoming connections.
  * The **`client_fd`** (Client Socket) is the dedicated line. When the server accepts a new user, it creates a temporary and exclusive `client_fd` to exchange data with that specific client.
* **Connection Setup (`bind()` and `listen()`)**: For the `server_fd` to work, it must first use the **`bind()`** function, which ties the socket to an IP address and a physical port (e.g., `8080`). Once bound, the **`listen()`** function is called, activating the socket and telling the OS to queue incoming calls on that port.
* **The Non-Blocking Heart (`poll()`)**: The server uses I/O multiplexing via the **`poll()`** function and sockets strictly configured as non-blocking (`O_NONBLOCK`). Instead of freezing while waiting for a slow client, `poll()` simultaneously monitors the entire list of File Descriptors (`server_fd` and multiple `client_fd`s), notifying the program *only* when a socket is ready to act.
* **The State Machine (`recv()` and `send()`)**: Since the server is non-blocking, it reads and writes in chunks:
  * **`recv()`**: When `poll()` signals that a client is sending data (`POLLIN`), `recv()` is used to read the raw data fragments arriving through the network from the `client_fd`, storing them in a buffer.
  * **`send()`**: Once the response is generated, when `poll()` signals that the client's socket is ready to receive (`POLLOUT`), `send()` transmits the response over the network to the browser, sending as many bytes as the OS allows in each cycle.

### 2. The Configuration File (`.conf`)
The configuration file is the "instruction manual" the server reads at startup to know how it should behave.

* **Block and Directive Parsing**: The `ConfigParser` class uses maps of function pointers (`_serverParsers` and `_locationParsers`) to initialize and delegate the reading of each directive[cite: 1].
* **Global Server Configuration**: Defines vital parameters processed by specific functions, such as:
  * `listen`: Determines the host and port where the server will apply the `bind()`[cite: 2].
  * `server_name`: Allows hosting multiple virtual websites on the same port[cite: 2].
  * `error_page`: Assigns paths to custom HTML files for specific HTTP error codes[cite: 2].
* **Routing Configuration (Locations)**: Using C++ templates, the parser assigns shared directives (like `root`, `index`, or `client_max_body_size`) to both main server blocks and specific routes (locations)[cite: 1].

### 3. HTTP Protocol Processing (`HttpRequest` and `HttpResponse`)
When `recv()` retrieves text from the network, the server must translate that raw text string into logical structures it can handle and reply to.

* **`HTTP Request`**: This is the structured message sent by the browser to the server. It always consists of a start line (e.g., `GET /index.html HTTP/1.1`), headers with browser metadata, and an optional body for data uploads. The server parses this in `HttpRequest.cpp`[cite: 4].
* **`HTTP Response`**: This is the server's structured reply to the client. It starts with a status line (e.g., `HTTP/1.1 200 OK` or `404 Not Found`), followed by its own headers (like `Content-Type: text/html`) and the payload. It is generated and encapsulated in `HttpResponse.cpp`[cite: 4].
* **Lifecycle and `keepalive()`**: In HTTP/1.1, the TCP connection is not immediately destroyed after the first `send()`. The **`keep-alive`** concept dictates that the server will keep the `client_fd` open for extra time (defined by directives like `keepalive_timeout`[cite: 2]). This allows the browser to request more files over the same channel, saving network overhead.
* **Transfer-Encoding and Pipelining**: The request processor handles fragmented bodies (*chunked*) and requests sent in bursts over the same socket (*pipelining*), safely extracting only the parsed bytes and leaving the rest intact in the buffer.

### 4. The Router and HTTP Methods (`Router`)
The `Router` takes the validated `HTTP Request`, compares it with the `.conf` settings, and decides what physical action to execute on the hard drive to populate the `HTTP Response`[cite: 3].

* **Location Matching**: The `matchLocation` function searches for the configured route that best matches the requested URI[cite: 3].
* **Security Filters**: Verifies if the HTTP method is allowed (`isMethodAllowed`) and that the body does not exceed `clientMaxBodySize`, preventing memory crashes[cite: 3].
* **GET Method Handling**: Resolves the physical path[cite: 3]. If it's a file, it reads it[cite: 3]. If it's a folder, it looks for an `index` or generates an interactive HTML directory listing if `autoindex` is on[cite: 3].
* **POST Method Handling**: Parses complex uploads (`multipart/form-data`) and safely writes the file to disk using `std::ofstream`, returning **201 Created**[cite: 3].
* **DELETE Method Handling**: Checks permissions and deletes the physical resource by invoking the POSIX `unlink` function, confirming the deletion with **204 No Content**[cite: 3].

### 5. Common Gateway Interface (CGI)
The server can break its static nature by generating content on the fly through external script delegation.

* **Detection and Execution**: If the router detects a request for a CGI file (such as `.py` or `.sh`)[cite: 4], it hands control over to the `CGIHandler`[cite: 3].
* **Isolation and Processes**: When running scripts (like `infinite.py` or `timeout.py`)[cite: 4], the architecture uses `fork()` to branch the server into child processes. This ensures that an infinite script does not block the parent process (the original `server_fd` and the `poll()` loop), keeping the main server operational for other clients.

### 6. Architectural State Machine
To efficiently handle multiple concurrent connections in a single event loop, the server maps the lifecycle of each connection into a strict State Machine[cite: 5].

* **`ACCEPTING`**: The `server_fd` acknowledges a new incoming connection and creates a new non-blocking socket[cite: 5].
* **`READING_REQUEST`**: The server uses `recv()` to accumulate raw bytes from the network into a connection-specific read buffer until the HTTP request is complete[cite: 5].
* **`PROCESSING`**: A stateless handler applies the read-only configuration to the parsed request, resolving static files or delegating to the CGI executor[cite: 5].
* **`WRITING_RESPONSE`**: The server uses `send()` to transmit the generated response back to the client from a dedicated write buffer[cite: 5].
* **`CLOSING`**: The socket is destroyed, unless the connection is kept alive for future requests[cite: 5].

### 7. Error Handling Taxonomy & Resilience
The core principle of the server is that it must never crash; all errors are caught, logged, and handled gracefully[cite: 5].

* **Fatal Errors (Configuration)**: Invalid syntax or missing required directives prevent the server from starting[cite: 5]. The validation occurs strictly during parsing, making the configuration read-only and thread-safe during runtime[cite: 5].
* **Network Errors (Non-Fatal)**: Failures in socket creation, `bind()`, `accept()`, or read/write operations result in closing the affected connection while continuing to serve others[cite: 5].
* **Resource Exhaustion (Non-Fatal)**: Issues like running out of memory, hitting the open connections limit, or a full disk result in rejecting new connections until resources become available[cite: 5].
* **Execution Errors (Non-Fatal)**: Unhandled exceptions or CGI script crashes are caught by the server, generating a default **500 Internal Server Error** response[cite: 5].

### 8. Memory Management in C++98 (RAII)
To ensure the strict "zero memory leaks" policy required by the project, the server relies on manual memory management paired with specific design patterns[cite: 5].

* **Resource Acquisition Is Initialization (RAII)**: Critical resources like sockets, file descriptors, and dynamic memory are wrapped inside C++ classes[cite: 5]. Constructors acquire the resources, and destructors release them, ensuring cleanups even if an exception is thrown[cite: 5].
* **Container Management**: Standard containers like `std::vector` and `std::map` are used for automatic memory management of configurations and tracking active connections, avoiding the use of raw arrays[cite: 5].

---

## 🇪🇸 Versión en Español

### 1. ¿Qué es el Servidor y cómo se inicializa?
Un servidor web es fundamentalmente un programa diseñado para escuchar peticiones de red, procesarlas y devolver respuestas válidas bajo el protocolo HTTP. Para lograr esto, se apoya en la API de *sockets* del sistema operativo, gestionada en archivos como `Server.cpp` y `Client.cpp`[cite: 4].

* **Sockets y File Descriptors (`server_fd` y `client_fd`)**: Un File Descriptor (FD) es un número que usa el sistema para identificar un recurso de red. 
  * El **`server_fd`** (Socket del Servidor) es el teléfono principal de la empresa. Se crea al arrancar y su única función es quedarse escuchando y aceptar conexiones nuevas.
  * El **`client_fd`** (Socket del Cliente) es la línea dedicada. Cuando el servidor acepta a un nuevo usuario, crea un `client_fd` temporal y exclusivo para intercambiar datos con ese cliente.
* **Preparación de la Conexión (`bind()` y `listen()`)**: Para que el `server_fd` funcione, primero debe usar la función **`bind()`**, que sirve para atar o enlazar el socket a una dirección IP y a un puerto físico (ej. `8080`). Una vez atado, se usa la función **`listen()`**, que activa el socket y le dice al sistema operativo: *"Estoy listo para recibir llamadas entrantes en este puerto y mantenerlas en una cola"*.
* **El Corazón No Bloqueante (`poll()`)**: El servidor utiliza multiplexación de I/O mediante la función **`poll()`** y sockets configurados como no bloqueantes (`O_NONBLOCK`). En lugar de congelarse esperando a un cliente lento, `poll()` vigila toda la lista de File Descriptors (`server_fd` y los múltiples `client_fd`) simultáneamente, avisando al programa *únicamente* cuando un socket está listo para actuar.
* **La Máquina de Estados (`recv()` y `send()`)**: Como no nos bloqueamos, leemos y escribimos a trozos:
  * **`recv()`**: Cuando `poll()` avisa que un cliente está hablando (`POLLIN`), usamos `recv()` para recibir (leer) del `client_fd` los fragmentos de datos crudos que hayan llegado por la red, guardándolos en un búfer.
  * **`send()`**: Una vez generada la respuesta, cuando `poll()` avisa que el socket del cliente no está saturado (`POLLOUT`), usamos `send()` para enviar la respuesta por la red hacia el navegador, enviando los bytes que el sistema operativo nos permita en cada ciclo.

### 2. El Archivo de Configuración (`.conf`)
El archivo de configuración es el "manual de instrucciones" que el servidor lee al arrancar para saber cómo debe comportarse.

* **Parseo de Bloques y Directivas**: La clase `ConfigParser` utiliza mapas de punteros a funciones (`_serverParsers` y `_locationParsers`) para inicializar y delegar la lectura de cada directiva[cite: 1].
* **Configuración Global del Servidor**: Define parámetros vitales procesados mediante funciones específicas, tales como:
  * `listen`: Determina el host y el puerto donde el servidor aplicará el `bind()`[cite: 2].
  * `server_name`: Permite alojar múltiples sitios web virtuales en el mismo puerto[cite: 2].
  * `error_page`: Asigna rutas de archivos HTML personalizados para códigos de error HTTP[cite: 2].
* **Configuración de Rutas (Locations)**: Utilizando plantillas de C++ (`templates`), el parser asigna directivas compartidas (como `root`, `index` o `client_max_body_size`) tanto a bloques principales de servidor como a rutas (locations) específicas[cite: 1].

### 3. Procesamiento del Protocolo HTTP (`HttpRequest` y `HttpResponse`)
Cuando el `recv()` obtiene texto de la red, el servidor debe traducir esa cadena de texto cruda a estructuras lógicas que pueda manejar y contestar.

* **`HTTP Request` (La Petición)**: Es el mensaje estructurado que envía el navegador al servidor. Se compone siempre de una línea de inicio (ej. `GET /index.html HTTP/1.1`), cabeceras (*headers*) con metadatos del navegador, y un cuerpo (*body*) opcional para subida de datos. El servidor parsea todo esto en el archivo `HttpRequest.cpp`[cite: 4].
* **`HTTP Response` (La Respuesta)**: Es la contestación estructurada del servidor hacia el cliente. Empieza con una línea de estado (ej. `HTTP/1.1 200 OK` o `404 Not Found`), seguida de cabeceras propias (como `Content-Type: text/html`) y el contenido (el HTML, la imagen, etc.). Se genera y encapsula en `HttpResponse.cpp`[cite: 4].
* **El Ciclo de Vida y `keepalive()`**: En el protocolo HTTP/1.1, la conexión TCP no se destruye inmediatamente tras el primer `send()`. El concepto de **`keep-alive`** indica que el servidor mantendrá el `client_fd` abierto durante un tiempo extra (definido en directivas como `keepalive_timeout`[cite: 2]). Esto permite que el navegador del usuario pida más archivos (como imágenes o CSS) usando el mismo canal, ahorrando el inmenso tiempo de red que costaría hacer un nuevo `bind()`, `listen()` y establecer una conexión nueva.
* **Transfer-Encoding y Pipelining**: El procesador de peticiones maneja cuerpos fragmentados (*chunked*) y peticiones enviadas en ráfaga por el mismo socket (*pipelining*), recortando únicamente los bytes utilizados y dejando el resto intacto en el búfer.

### 4. El Enrutador y los Métodos HTTP (`Router`)
El `Router` toma el `HTTP Request` validado, lo compara con la configuración cargada del `.conf` y decide qué acción física ejecutar en el disco duro o sistema para rellenar el `HTTP Response`[cite: 3].

* **Asignación de Ruta (Location Matching)**: La función `matchLocation` busca la ruta configurada que mejor coincida con la URI solicitada por el cliente[cite: 3].
* **Filtros de Seguridad**: Verifica si el método HTTP está permitido (`isMethodAllowed`) y que el cuerpo no exceda `clientMaxBodySize`, previniendo así caídas de memoria[cite: 3].
* **Manejo del Método GET**: Resuelve la ruta física[cite: 3]. Si es un archivo, lo lee[cite: 3]. Si es una carpeta, busca un `index` o genera un listado HTML interactivo de su contenido si `autoindex` está encendido[cite: 3].
* **Manejo del Método POST**: Analiza subidas complejas (`multipart/form-data`) y escribe el archivo de forma segura en el disco duro usando `std::ofstream`, devolviendo **201 Created**[cite: 3].
* **Manejo del Método DELETE**: Comprueba permisos y elimina el recurso físico invocando la función POSIX `unlink`, confirmando el borrado con **204 No Content**[cite: 3].

### 5. Interfaz de Puerta de Enlace Común (CGI)
El servidor también es capaz de romper su naturaleza estática, generando contenido al vuelo mediante delegación.

* **Detección y Ejecución**: Si el enrutador detecta una solicitud hacia un archivo CGI (como `.py` o `.sh`)[cite: 4], cede el control a `CGIHandler`[cite: 3].
* **Aislamiento y Procesos**: Al ejecutarse scripts (como `infinite.py` o `timeout.py`)[cite: 4], la arquitectura utiliza `fork()` para bifurcar el servidor en procesos hijos. Esto garantiza que un script infinito no bloquee al padre (el `server_fd` original y el bucle de `poll()`), manteniendo el servidor principal operativo para el resto de clientes de la red.

### 6. Máquina de Estados Arquitectónica
Para manejar eficientemente múltiples conexiones concurrentes en un solo bucle de eventos, el servidor mapea el ciclo de vida de cada conexión en una rigurosa Máquina de Estados[cite: 5].

* **`ACCEPTING` (Aceptando)**: El `server_fd` reconoce una nueva conexión entrante y crea un nuevo socket no bloqueante[cite: 5].
* **`READING_REQUEST` (Leyendo Petición)**: El servidor usa `recv()` para acumular bytes crudos de la red en un búfer de lectura específico de la conexión hasta que la petición HTTP está completa[cite: 5].
* **`PROCESSING` (Procesando)**: Un controlador sin estado (stateless) aplica la configuración de solo lectura a la petición parseada, resolviendo archivos estáticos o delegando la tarea al ejecutor CGI[cite: 5].
* **`WRITING_RESPONSE` (Escribiendo Respuesta)**: El servidor usa `send()` para transmitir la respuesta generada de vuelta al cliente desde un búfer de escritura dedicado[cite: 5].
* **`CLOSING` (Cerrando)**: El socket se destruye, a menos que la conexión se mantenga viva (`keep-alive`) para futuras peticiones[cite: 5].

### 7. Taxonomía de Errores y Resiliencia
El principio central del servidor es que nunca debe colapsar; todos los errores se capturan, se registran y se manejan con elegancia[cite: 5].

* **Errores Fatales (Configuración)**: La sintaxis inválida o la falta de directivas obligatorias evitan que el servidor arranque[cite: 5]. La validación ocurre estrictamente durante el parseo, haciendo que la configuración sea de solo lectura y segura durante la ejecución[cite: 5].
* **Errores de Red (No Fatales)**: Los fallos en la creación de sockets, `bind()`, `accept()` o en las operaciones de lectura/escritura provocan el cierre de la conexión afectada, pero el servidor sigue atendiendo a las demás[cite: 5].
* **Agotamiento de Recursos (No Fatal)**: Problemas como quedarse sin memoria, alcanzar el límite de conexiones abiertas o un disco lleno resultan en el rechazo de nuevas conexiones hasta que haya recursos disponibles[cite: 5].
* **Errores de Ejecución (No Fatales)**: Las excepciones no controladas o las caídas de scripts CGI son capturadas por el servidor, generando una respuesta por defecto de **500 Internal Server Error**[cite: 5].

### 8. Gestión de Memoria en C++98 (RAII)
Para garantizar la estricta política de "cero fugas de memoria" (*zero memory leaks*) exigida por el proyecto, el servidor se apoya en la gestión manual de memoria combinada con patrones de diseño específicos[cite: 5].

* **Resource Acquisition Is Initialization (RAII)**: Los recursos críticos como sockets, descriptores de archivos y memoria dinámica se envuelven en clases de C++[cite: 5]. Los constructores adquieren los recursos y los destructores los liberan, asegurando la limpieza incluso si se lanza una excepción[cite: 5].
* **Gestión mediante Contenedores**: Se utilizan contenedores estándar como `std::vector` y `std::map` para la gestión automática de memoria de las configuraciones y el rastreo de conexiones activas, evitando el uso de arrays crudos[cite: 5].

![alt text](image.png)