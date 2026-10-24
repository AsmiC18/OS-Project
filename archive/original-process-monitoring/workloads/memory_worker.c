
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

static int parse_size_mb(const char *text, size_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value == 0 || value > 256) {
        return -1;
    }

    *out = (size_t)value;
    return 0;
}

static int parse_interval_ms(const char *text, long *out)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value < 10 || value > 10000) {
        return -1;
    }

    *out = value;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <maximum-memory-MB:1-256> <interval-ms:10-10000>\n",
                argv[0]);
        return 2;
    }

    size_t maximum_mb;
    long interval_ms;

    if (parse_size_mb(argv[1], &maximum_mb) != 0 ||
        parse_interval_ms(argv[2], &interval_ms) != 0) {
        fprintf(stderr, "Invalid arguments. Memory must be 1-256 MB "
                        "and interval must be 10-10000 ms.\n");
        return 2;
    }

    const size_t chunk_size = 1024 * 1024;
    unsigned char **chunks = calloc(maximum_mb, sizeof(*chunks));

    if (chunks == NULL) {
        perror("calloc");
        return 1;
    }

    printf("memory_worker: maximum=%zu MB interval=%ld ms\n",
           maximum_mb, interval_ms);
    fflush(stdout);

    size_t allocated = 0;

    while (allocated < maximum_mb) {
        unsigned char *chunk = malloc(chunk_size);
        if (chunk == NULL) {
            perror("malloc");
            break;
        }

        /* Touch each page so physical memory use becomes observable. */
        long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            free(chunk);
            fprintf(stderr, "Unable to determine system page size.\n");
            break;
        }

        for (size_t offset = 0; offset < chunk_size;
             offset += (size_t)page_size) {
            chunk[offset] = (unsigned char)(allocated + offset);
        }

        chunks[allocated] = chunk;
        allocated++;

        printf("memory_worker: allocated=%zu MB\n", allocated);
        fflush(stdout);

        if (allocated < maximum_mb) {
            struct timespec delay = {
                .tv_sec = interval_ms / 1000,
                .tv_nsec = (interval_ms % 1000) * 1000000L
            };

            while (nanosleep(&delay, &delay) != 0) {
                if (errno != EINTR) {
                    perror("nanosleep");
                    goto cleanup;
                }
            }
        }
    }

    printf("memory_worker: reached %zu MB; holding for 2 seconds\n",
           allocated);
    fflush(stdout);

    {
        struct timespec hold = {.tv_sec = 2, .tv_nsec = 0};
        while (nanosleep(&hold, &hold) != 0) {
            if (errno != EINTR) {
                perror("nanosleep");
                break;
            }
        }
    }

cleanup:
    for (size_t i = 0; i < allocated; i++) {
        free(chunks[i]);
    }
    free(chunks);

    printf("memory_worker: released allocated memory and exiting\n");
    return allocated == maximum_mb ? 0 : 1;
}
