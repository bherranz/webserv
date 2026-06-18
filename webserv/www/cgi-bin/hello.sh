#!/bin/bash
echo "Content-Type: text/plain"
echo "Status: 200 OK"
echo ""
echo "Hello from Shell CGI!"
echo "Method: $REQUEST_METHOD"
echo "Query: $QUERY_STRING"
