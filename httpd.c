/* tinyhttpd - a tiny HTTP server for learning purposes
 * Based on J. David Blackstone's original tinyhttpd
 *
 * Compile: gcc -o httpd httpd.c -lpthread
 * Usage:   ./httpd [port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <stdarg.h>

#define SERVER_STRING "Server: tinyhttpd/0.1.0\r\n"
#define HTDOCS_ROOT   "htdocs"
#define BUF_SIZE      1024
#define DEFAULT_PORT  8080

/* ---------- helper prototypes ---------- */
static void *accept_request(void *arg);
static void  bad_request(int client);
static void  cat(int client, FILE *fp);
static void  cannot_execute(int client);
static void  error_die(const char *sc);
static void  execute_cgi(int client, const char *path,
                         const char *method, const char *query_string);
static int   get_line(int sock, char *buf, int size);
static void  headers(int client, const char *filename);
static void  not_found(int client);
static void  serve_file(int client, const char *filename);
static int   startup(uint16_t *port);
static void  unimplemented(int client);

/* ======================================================
 *  Main
 * ====================================================== */
int main(int argc, char *argv[])
{
    uint16_t port = DEFAULT_PORT;

    if (argc == 2)
        port = (uint16_t)atoi(argv[1]);

    int server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while (1) {
        int client_sock = accept(server_sock,
                                 (struct sockaddr *)&client_addr,
                                 &client_addr_len);
        if (client_sock == -1)
            error_die("accept");

        pthread_t newthread;
        int *pclient = malloc(sizeof(int));
        if (!pclient)
            error_die("malloc");
        *pclient = client_sock;

        if (pthread_create(&newthread, NULL, accept_request,
                           (void *)pclient) != 0)
            error_die("pthread_create");

        pthread_detach(newthread);
    }

    close(server_sock);
    return 0;
}

/* ======================================================
 *  startup – create, bind, and listen on a TCP socket.
 *  If *port == 0 the OS picks an ephemeral port and
 *  *port is updated with the chosen value.
 * ====================================================== */
static int startup(uint16_t *port)
{
    int httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");

    int on = 1;
    if (setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        perror("setsockopt SO_REUSEADDR");

    struct sockaddr_in name;
    memset(&name, 0, sizeof(name));
    name.sin_family      = AF_INET;
    name.sin_port        = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");

    /* If port 0 was requested, find out what was assigned */
    if (*port == 0) {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }

    if (listen(httpd, 5) < 0)
        error_die("listen");

    return httpd;
}

/* ======================================================
 *  accept_request – handle one HTTP request / response
 *  cycle on the connected socket.
 * ====================================================== */
static void *accept_request(void *arg)
{
    int client = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    char method[16];
    char url[4096];
    char path[4200];
    size_t i, j;
    struct stat st;
    int cgi = 0;
    char *query_string = NULL;

    /* ---- read the request line ---- */
    int numchars = get_line(client, buf, sizeof(buf));

    /* extract method */
    i = 0; j = 0;
    while (!isspace((unsigned char)buf[j]) && j < sizeof(method) - 1)
        method[i++] = buf[j++];
    method[i] = '\0';

    if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "POST") != 0) {
        unimplemented(client);
        close(client);
        return NULL;
    }

    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    /* skip whitespace */
    while (isspace((unsigned char)buf[j]) && j < (size_t)numchars)
        j++;

    /* extract URL */
    i = 0;
    while (!isspace((unsigned char)buf[j]) && i < sizeof(url) - 1 &&
           j < (size_t)numchars)
        url[i++] = buf[j++];
    url[i] = '\0';

    /* for GET, split query string from path */
    if (strcasecmp(method, "GET") == 0) {
        query_string = url;
        while (*query_string != '?' && *query_string != '\0')
            query_string++;
        if (*query_string == '?') {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    /* build filesystem path */
    snprintf(path, sizeof(path), "%s%s", HTDOCS_ROOT, url);
    if (strlen(path) > 0 && path[strlen(path) - 1] == '/')
        strncat(path, "index.html", sizeof(path) - strlen(path) - 1);

    /* drain remaining request headers */
    if (!cgi) {
        while ((numchars > 0) && strcmp("\n", buf))
            numchars = get_line(client, buf, sizeof(buf));
    }

    /* check whether file exists */
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf))
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    } else {
        if (S_ISDIR(st.st_mode))
            strncat(path, "/index.html", sizeof(path) - strlen(path) - 1);

        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH))
            cgi = 1;

        if (!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string ? query_string : "");
    }

    close(client);
    return NULL;
}

/* ======================================================
 *  get_line – read one HTTP header line from socket.
 *  Converts \r\n -> \n.  Returns number of bytes stored
 *  (including the trailing \n but not \0).
 * ====================================================== */
static int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';

    while (i < size - 1 && c != '\n') {
        int n = recv(sock, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);
                if (n > 0 && c == '\n')
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i++] = c;
        } else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

/* ======================================================
 *  headers – send a minimal HTTP/1.0 200 OK header for
 *  the given filename (derives Content-Type from ext).
 * ====================================================== */
static void headers(int client, const char *filename)
{
    const char *content_type = "text/plain";

    const char *ext = strrchr(filename, '.');
    if (ext) {
        if      (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
            content_type = "text/html";
        else if (strcasecmp(ext, ".css")  == 0)
            content_type = "text/css";
        else if (strcasecmp(ext, ".js")   == 0)
            content_type = "application/javascript";
        else if (strcasecmp(ext, ".png")  == 0)
            content_type = "image/png";
        else if (strcasecmp(ext, ".jpg")  == 0 || strcasecmp(ext, ".jpeg") == 0)
            content_type = "image/jpeg";
        else if (strcasecmp(ext, ".gif")  == 0)
            content_type = "image/gif";
    }

    char buf[1024];
    snprintf(buf, sizeof(buf),
             "HTTP/1.0 200 OK\r\n"
             "%s"
             "Content-Type: %s\r\n"
             "\r\n",
             SERVER_STRING, content_type);
    send(client, buf, strlen(buf), 0);
}

/* ======================================================
 *  serve_file – send a static file to the client.
 * ====================================================== */
static void serve_file(int client, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        not_found(client);
        return;
    }

    headers(client, filename);
    cat(client, fp);
    fclose(fp);
}

/* ======================================================
 *  cat – read a FILE and write its contents to the socket.
 * ====================================================== */
static void cat(int client, FILE *fp)
{
    char buf[BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        send(client, buf, n, 0);
}

/* ======================================================
 *  execute_cgi – fork a child, set up CGI environment
 *  variables, and pipe stdout back to the client.
 * ====================================================== */
static void execute_cgi(int client, const char *path,
                        const char *method, const char *query_string)
{
    char buf[BUF_SIZE];
    int cgi_output[2];   /* parent reads  [0], child writes [1] */
    int cgi_input[2];    /* child reads   [0], parent writes [1] */
    pid_t pid;
    int status;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';

    if (strcasecmp(method, "GET") == 0) {
        while (numchars > 0 && strcmp("\n", buf))
            numchars = get_line(client, buf, sizeof(buf));
    } else {
        /* POST: find Content-Length header */
        while (numchars > 0 && strcmp("\n", buf)) {
            numchars = get_line(client, buf, sizeof(buf));
            if (strncasecmp(buf, "Content-Length:", 15) == 0)
                content_length = atoi(buf + 16);
        }
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }

    /* send HTTP/1.0 200 OK — CGI script provides its own Content-Type */
    snprintf(buf, sizeof(buf),
             "HTTP/1.0 200 OK\r\n"
             "%s", SERVER_STRING);
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0 || pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    pid = fork();
    if (pid < 0) {
        cannot_execute(client);
        return;
    }

    if (pid == 0) {
        /* ---- child: become the CGI script ---- */
        char meth_env[32], length_env[64];

        /* redirect stdin / stdout */
        dup2(cgi_output[1], STDOUT_FILENO);
        dup2(cgi_input[0],  STDIN_FILENO);
        close(cgi_output[0]);
        close(cgi_input[1]);

        snprintf(meth_env, sizeof(meth_env), "REQUEST_METHOD=%s", method);
        putenv(meth_env);

        if (strcasecmp(method, "GET") == 0) {
            setenv("QUERY_STRING", query_string, 1);
        } else {
            snprintf(length_env, sizeof(length_env),
                     "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }

        execl(path, path, NULL);
        /* execl only returns on error */
        exit(1);
    } else {
        /* ---- parent: relay data ---- */
        char rbuf[BUF_SIZE];
        ssize_t nread;

        close(cgi_output[1]);
        close(cgi_input[0]);

        if (strcasecmp(method, "POST") == 0) {
            int remaining = content_length;
            while (remaining > 0) {
                int chunk = remaining < (int)sizeof(rbuf) ? remaining : (int)sizeof(rbuf);
                nread = recv(client, rbuf, chunk, 0);
                if (nread <= 0)
                    break;
                write(cgi_input[1], rbuf, nread);
                remaining -= (int)nread;
            }
        }

        while ((nread = read(cgi_output[0], rbuf, sizeof(rbuf))) > 0)
            send(client, rbuf, nread, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}

/* ======================================================
 *  Error responses
 * ====================================================== */
static void bad_request(int client)
{
    const char *body =
        "HTTP/1.0 400 BAD REQUEST\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><head><title>400 Bad Request</title></head>\r\n"
        "<body><h1>Bad Request</h1>\r\n"
        "<p>Your browser sent a request this server could not understand.</p>\r\n"
        "</body></html>\r\n";
    send(client, body, strlen(body), 0);
}

static void not_found(int client)
{
    const char *body =
        "HTTP/1.0 404 NOT FOUND\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><head><title>404 Not Found</title></head>\r\n"
        "<body><h1>Not Found</h1>\r\n"
        "<p>The requested URL was not found on this server.</p>\r\n"
        "</body></html>\r\n";
    send(client, body, strlen(body), 0);
}

static void unimplemented(int client)
{
    const char *body =
        "HTTP/1.0 501 Method Not Implemented\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><head><title>501 Not Implemented</title></head>\r\n"
        "<body><h1>Method Not Implemented</h1>\r\n"
        "<p>tinyhttpd does not support that method.</p>\r\n"
        "</body></html>\r\n";
    send(client, body, strlen(body), 0);
}

static void cannot_execute(int client)
{
    const char *body =
        "HTTP/1.0 500 Internal Server Error\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><head><title>500 Internal Server Error</title></head>\r\n"
        "<body><h1>Internal Server Error</h1>\r\n"
        "<p>CGI script could not be executed.</p>\r\n"
        "</body></html>\r\n";
    send(client, body, strlen(body), 0);
}

/* ======================================================
 *  error_die – print an error and abort the process.
 * ====================================================== */
static void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}
