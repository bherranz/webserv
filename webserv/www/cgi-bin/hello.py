import os

# 1. El script DEBE imprimir las cabeceras requeridas seguidas de una línea en blanco
print("Content-Type: text/html")
print("Status: 200 OK")
print("")

# 2. El Body
print("<html><body>")
print("<h1>¡Hola desde Python!</h1>")
print("<p>El método usado fue: " + os.environ.get("REQUEST_METHOD", "Desconocido") + "</p>")
print("</body></html>")