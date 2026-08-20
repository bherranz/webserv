import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("localhost", 8080))

# Enviamos las cabeceras completas
s.sendall(b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n")

# Enviamos el body "goteando" 1 byte por segundo
body = b"0123456789"
for byte in body:
    print(f"Enviando byte: {chr(byte)}")
    s.send(bytes([byte]))
    time.sleep(1) # Dormimos 1 segundo entre cada byte

print("\nEsperando respuesta del servidor...")
print(s.recv(1024).decode())
s.close()