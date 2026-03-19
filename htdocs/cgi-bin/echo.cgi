#!/bin/bash
# CGI script: echo POST body (POST)
# Reads CONTENT_LENGTH bytes from stdin, decodes "msg=<value>", and echoes it.

echo "Content-Type: text/html"
echo ""

CONTENT_LENGTH="${CONTENT_LENGTH:-0}"
BODY=$(dd bs=1 count="$CONTENT_LENGTH" 2>/dev/null)

# Extract "msg" field (simple URL-decode: replace + with space)
MSG=$(echo "$BODY" | sed 's/.*msg=\([^&]*\).*/\1/' | tr '+' ' ')

if [ -z "$MSG" ]; then
  MSG="(empty)"
fi

cat <<EOF
<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>Echo</title>
<style>body{font-family:sans-serif;max-width:500px;margin:40px auto;padding:0 20px;}</style>
</head>
<body>
  <h1>Echo</h1>
  <p>You sent: <strong>$MSG</strong></p>
  <p><a href="/">← Back</a></p>
</body>
</html>
EOF
