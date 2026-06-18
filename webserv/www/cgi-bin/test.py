#!/usr/bin/env python3
import os

print("Content-Type: text/html")
print("Status: 200 OK")
print()
print("<html><body>")
print("<h1>CGI Test</h1>")
print("<p>Method: {}</p>".format(os.environ.get("REQUEST_METHOD", "unknown")))
print("<p>Query: {}</p>".format(os.environ.get("QUERY_STRING", "none")))
print("<p>Script: {}</p>".format(os.environ.get("SCRIPT_NAME", "unknown")))
print("<p>Body length: {}</p>".format(os.environ.get("CONTENT_LENGTH", "0")))
print("</body></html>")
