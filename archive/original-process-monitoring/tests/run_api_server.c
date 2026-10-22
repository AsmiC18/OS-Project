
#include "api_server.h"

#include <stdio.h>

int main(void)
{
    puts("API server listening on 127.0.0.1:18080");
    fflush(stdout);

    if (api_server_serve_once(18080) != 0) {
        perror("api_server_serve_once");
        return 1;
    }

    return 0;
}
