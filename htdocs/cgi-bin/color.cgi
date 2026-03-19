#!/bin/bash
# CGI script: colour lookup (GET)
# Reads QUERY_STRING, parses "color=<name>", responds with a coloured swatch.

echo "Content-Type: text/html"
echo ""

# Parse QUERY_STRING (simple key=value, URL-decode + only)
RAW="${QUERY_STRING}"
COLOR=$(echo "$RAW" | sed 's/.*color=\([^&]*\).*/\1/' | tr '+' ' ')

if [ -z "$COLOR" ]; then
  COLOR="(none)"
fi

cat <<EOF
<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>Color: $COLOR</title>
<style>body{font-family:sans-serif;max-width:500px;margin:40px auto;padding:0 20px;}</style>
</head>
<body>
  <h1>Color lookup</h1>
  <p>You asked for: <strong>$COLOR</strong></p>
  <div style="width:200px;height:100px;background:$COLOR;border:1px solid #ccc;"></div>
  <p><a href="/">← Back</a></p>
</body>
</html>
EOF
