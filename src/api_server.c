
#include "api_server.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define CLOSE_SOCKET(s) closesocket(s)
#define SOCKET_ERROR_VALUE INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSE_SOCKET(s) close(s)
#define SOCKET_ERROR_VALUE (-1)
#endif

#define REQUEST_MAX 2048

static int send_all(socket_t client, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
#ifdef _WIN32
        int n = send(client, data + sent,
                     (int)(length - sent), 0);
#else
        ssize_t n = send(client, data + sent, length - sent, 0);
#endif
        if (n <= 0)
            return -1;
        sent += (size_t)n;
    }

    return 0;
}

static int send_response(socket_t client, int status,
                         const char *reason, const char *body)
{
    char response[512];

    int n = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "\r\n%s",
        status, reason, (unsigned long)strlen(body), body);

    if (n < 0 || (size_t)n >= sizeof(response)) {
        errno = EINVAL;
        return -1;
    }

    return send_all(client, response, (size_t)n);
}

int api_server_serve_once(unsigned short port)
{
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        errno = EIO;
        return -1;
    }
#endif

    int result = -1;
    socket_t server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

   if (server == SOCKET_ERROR_VALUE)
    goto cleanup;

#ifdef _WIN32
    BOOL reuse = TRUE;
#else
    int reuse = 1;
#endif

    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&reuse, sizeof(reuse)) != 0)
        goto close_server;

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, (struct sockaddr *)&address, sizeof(address)) != 0)
        goto close_server;

    if (listen(server, 1) != 0)
        goto close_server;

    socket_t client = accept(server, NULL, NULL);
    if (client == SOCKET_ERROR_VALUE)
        goto close_server;

    char request[REQUEST_MAX + 1];
    size_t used = 0;
    int complete = 0;

    while (used < REQUEST_MAX) {
#ifdef _WIN32
        int n = recv(client, request + used,
                     (int)(REQUEST_MAX - used), 0);
#else
        ssize_t n = recv(client, request + used,
                         REQUEST_MAX - used, 0);
#endif
        if (n <= 0)
            break;

        used += (size_t)n;
        request[used] = '\0';

        if (strstr(request, "\r\n\r\n")) {
            complete = 1;
            break;
        }
    }


    if (!complete) {
        if (send_response(client, 400, "Bad Request",
                          "{\"error\":\"invalid or oversized request\"}\n") == 0)
            result = 0;
    } else {
        char method[16] = {0};
        char path[128] = {0};
        char version[16] = {0};

        int fields = sscanf(request, "%15s %127s %15s",
                            method, path, version);

        if (fields != 3 ||
            (strcmp(version, "HTTP/1.1") != 0 &&
             strcmp(version, "HTTP/1.0") != 0)) {
            if (send_response(client, 400, "Bad Request",
                              "{\"error\":\"bad request line\"}\n") == 0)
                result = 0;
        } else if (strcmp(method, "GET") != 0) {
            if (send_response(client, 405, "Method Not Allowed",
                              "{\"error\":\"method not allowed\"}\n") == 0)
                result = 0;
        } else if (strcmp(path, "/health") != 0) {
            if (send_response(client, 404, "Not Found",
                              "{\"error\":\"route not found\"}\n") == 0)
                result = 0;
        } else {
            if (send_response(client, 200, "OK",
                              "{\"status\":\"ok\"}\n") == 0)
                result = 0;
        }
    }


    CLOSE_SOCKET(client);

close_server:
    CLOSE_SOCKET(server);

cleanup:
#ifdef _WIN32
    WSACleanup();
#endif
    return result;
}
