
#ifndef AEGISOS_API_SERVER_H
#define AEGISOS_API_SERVER_H

/*
 * Serve exactly one HTTP request on the loopback interface.
 * Supports GET /health only.
 * Returns 0 on success, -1 on failure and sets errno.
 */
int api_server_serve_once(unsigned short port);

#endif
