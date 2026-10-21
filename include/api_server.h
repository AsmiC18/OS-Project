
#ifndef AEGISOS_API_SERVER_H
#define AEGISOS_API_SERVER_H

/*
 * Serve exactly one HTTP request.
 * Used by the existing API tests.
 */
int api_server_serve_once(unsigned short port);

/*
 * Run the HTTP server continuously for the dashboard.
 * Returns -1 on failure.
 */
int api_server_run(unsigned short port);

#endif

