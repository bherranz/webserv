#!/usr/bin/env python3
import os
import sys

method = os.environ.get("REQUEST_METHOD", "GET")
body = sys.stdin.read()

print("Content-Type: text/html")
print("Status: 200 OK")
print()
print("<html><body>")
print("<h1>Form POST Test</h1>")
print("<p>Method: {}</p>".format(method))
print("<p>Body received ({} bytes): {}</p>".format(len(body), body))
print("</body></html>")
