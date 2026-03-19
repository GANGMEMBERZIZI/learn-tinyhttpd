# learn-tinyhttpd

A minimal HTTP server written in C, based on J. David Blackstone's classic
[tinyhttpd](http://tinyhttpd.sourceforge.net/) project.  The goal is to
understand the basics of:

* TCP sockets (`socket`, `bind`, `listen`, `accept`)
* HTTP/1.0 request parsing (method, URL, headers)
* Serving static files
* Executing CGI scripts via `fork`/`exec`
* Handling concurrent clients with POSIX threads

## File layout

```
.
├── httpd.c                  # Main server source
├── Makefile
└── htdocs/
    ├── index.html           # Default home page
    └── cgi-bin/
        ├── color.cgi        # GET demo – renders a colour swatch
        └── echo.cgi         # POST demo – echoes a form field
```

## Build

```bash
make
```

Requires GCC and POSIX threads (`-lpthread`).

## Run

```bash
./httpd [port]      # default port: 8080
```

Then open **http://localhost:8080/** in a browser.

## How it works

1. `startup()` creates a TCP socket, binds it, and starts listening.
2. The main loop calls `accept()` and spawns a detached thread for every
   incoming connection.
3. `accept_request()` reads the HTTP request line, extracts the method and URL,
   then either:
   * calls `serve_file()` for plain static files, or
   * calls `execute_cgi()` for executable files (the CGI scripts in
     `htdocs/cgi-bin/`), setting `REQUEST_METHOD`, `QUERY_STRING` /
     `CONTENT_LENGTH` environment variables and piping stdin/stdout.
4. Error responses (`400`, `404`, `501`, `500`) are plain HTML strings sent
   directly to the socket.

## Supported HTTP methods

| Method | Behaviour |
|--------|-----------|
| GET    | Serve a static file, or run CGI with `QUERY_STRING` |
| POST   | Run CGI, forwarding the body via stdin and `CONTENT_LENGTH` |
| Others | Return `501 Not Implemented` |
