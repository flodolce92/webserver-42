#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

print("""
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <title>Python CGI Test</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: #f2f2f2;
            color: #333;
            margin: 40px;
        }
        h1 {
            color: #0066cc;
        }
        h2 {
            color: #444;
            border-bottom: 2px solid #ddd;
            padding-bottom: 5px;
        }
        pre {
            background-color: #fff;
            border: 1px solid #ccc;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
        .container {
            max-width: 800px;
            margin: auto;
            background: #ffffff;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
    </style>
</head>
<body>
    <div class="container">
""")

query_string = os.environ.get("query_string", "")
params = parse_qs(query_string)
name = params.get('name', ['Guest'])[0]

print(f"        <h1>Hello {name} from Python CGI!</h1>")
print("        <h2>Environment Variables:</h2>")
print("        <pre>", end='')

for key, value in os.environ.items():
    print(f"{key}={value}")

print("</pre>")

if os.environ.get("request_method") == "POST":
    content_length = int(os.environ.get("content-length", 0))
    if content_length > 0:
        post_data = sys.stdin.read(content_length)
        print("<h2>POST Data:</h2>")
        print(f"<pre>{post_data}</pre>")
    else:
        print("<h2>No POST Data.</h2>")
else:
    print("<h2>Not a POST request.</h2>")

print("""
    </div>
</body>
</html>
""")
