
#define _POSIX_C_SOURCE 200809L

#include "api_server.h"
#include "anomaly.h"
#include "monitor.h"
#include "process.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>

#define REQUEST_MAX 2048
#define ANOMALY_HISTORY_CAPACITY 100

typedef struct {
    Anomaly anomaly;
    time_t timestamp;
} StoredAnomaly;

static ProcessMonitor process_monitor;
static AnomalyDetector anomaly_detector;
static int monitor_ready = 0;

static StoredAnomaly anomaly_history[ANOMALY_HISTORY_CAPACITY];
static size_t anomaly_count = 0;
static size_t anomaly_next = 0;

static int send_all(int client, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(client, data + sent, length - sent, 0);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

static int send_response(int client, int status,
                         const char *reason, const char *body)
{
    char header[512];

    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "\r\n",
        status, reason, strlen(body));

    if (n < 0 || (size_t)n >= sizeof(header))
        return -1;

    if (send_all(client, header, (size_t)n) != 0)
        return -1;

    return send_all(client, body, strlen(body));
}

static int read_request(int client, char *request, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity) {
        ssize_t n = recv(client, request + used,
                         capacity - used - 1, 0);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        used += (size_t)n;
        request[used] = '\0';

        if (strstr(request, "\r\n\r\n") != NULL)
            return 0;
    }

    return -1;
}

static void write_json_string(FILE *out, const char *text)
{
    fputc('"', out);

    for (const unsigned char *p = (const unsigned char *)text;
         *p != '\0'; ++p) {
        switch (*p) {
        case '"':
            fputs("\\\"", out);
            break;
        case '\\':
            fputs("\\\\", out);
            break;
        case '\n':
            fputs("\\n", out);
            break;
        case '\r':
            fputs("\\r", out);
            break;
        case '\t':
            fputs("\\t", out);
            break;
        default:
            if (*p >= 0x20)
                fputc(*p, out);
            break;
        }
    }

    fputc('"', out);
}

static void store_anomaly(const Anomaly *anomaly)
{
    anomaly_history[anomaly_next].anomaly = *anomaly;
    anomaly_history[anomaly_next].timestamp = time(NULL);

    anomaly_next = (anomaly_next + 1) % ANOMALY_HISTORY_CAPACITY;

    if (anomaly_count < ANOMALY_HISTORY_CAPACITY)
        anomaly_count++;
}

static const char *anomaly_type_name(AnomalyType type)
{
    switch (type) {
    case ANOMALY_HIGH_CPU:
        return "high_cpu";
    case ANOMALY_HIGH_MEMORY:
        return "high_memory";
    case ANOMALY_PROCESS_UNAVAILABLE:
        return "process_unavailable";
    default:
        return "unknown";
    }
}

static void evaluate_sample(const MonitorSample *sample)
{
    if (!monitor_ready || !sample->valid)
        return;

    Anomaly anomaly;
    int result = anomaly_detector_evaluate(
        &anomaly_detector, sample, &anomaly);

    if (result == 1)
        store_anomaly(&anomaly);
}

static int send_processes(int client)
{
    char *json = NULL;
    size_t json_length = 0;

    FILE *out = open_memstream(&json, &json_length);
    if (out == NULL) {
        return send_response(client, 500, "Internal Server Error",
            "{\"error\":\"could not create response\"}\n");
    }

    fputs("{\"processes\":[", out);

    DIR *proc = opendir("/proc");
    if (proc == NULL) {
        fclose(out);
        free(json);

        return send_response(client, 500, "Internal Server Error",
            "{\"error\":\"cannot read /proc\"}\n");
    }

    int first = 1;
    struct dirent *entry;

    while ((entry = readdir(proc)) != NULL) {
        char *end = NULL;

        errno = 0;
        long value = strtol(entry->d_name, &end, 10);

        if (errno != 0 || end == entry->d_name || *end != '\0' ||
            value <= 0 || value > INT_MAX)
            continue;

        AegisProcessInfo info;

        if (process_read((pid_t)value, &info) != 0)
            continue;

        double cpu_percent = 0.0;
        double memory_mb = 0.0;

        if (monitor_ready) {
            MonitorSample sample;

            if (monitor_sample(&process_monitor, (pid_t)value,
                               &sample) == 0) {
                cpu_percent = sample.cpu_percent;
                memory_mb = sample.memory_mb;
                evaluate_sample(&sample);
            }
        } else {
            long page_size = sysconf(_SC_PAGESIZE);

            if (page_size > 0 && info.resident_pages > 0) {
                memory_mb = (double)info.resident_pages *
                            (double)page_size / (1024.0 * 1024.0);
            }
        }

        if (!first)
            fputc(',', out);

        first = 0;

        fprintf(out, "{\"pid\":%ld,\"name\":", (long)info.pid);
        write_json_string(out, info.name);

        fprintf(out,
            ",\"cpu_percent\":%.2f,"
            "\"memory_mb\":%.2f,"
            "\"state\":",
            cpu_percent, memory_mb);

        char state[2] = { info.state, '\0' };
        write_json_string(out, state);

        fputc('}', out);
    }

    closedir(proc);
    fputs("]}\n", out);

    if (fclose(out) != 0) {
        free(json);
        return -1;
    }

    char header[256];

    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n",
        json_length);

    int result = -1;

    if (n > 0 && (size_t)n < sizeof(header) &&
        send_all(client, header, (size_t)n) == 0) {
        result = send_all(client, json, json_length);
    }

    free(json);
    return result;
}

static int send_anomalies(int client)
{
    char *json = NULL;
    size_t json_length = 0;

    FILE *out = open_memstream(&json, &json_length);
    if (out == NULL) {
        return send_response(client, 500, "Internal Server Error",
            "{\"error\":\"could not create response\"}\n");
    }

    fprintf(out, "{\"anomalies\":[");

    for (size_t i = 0; i < anomaly_count; i++) {
        size_t index;

        if (anomaly_count < ANOMALY_HISTORY_CAPACITY) {
            index = i;
        } else {
            index = (anomaly_next + i) % ANOMALY_HISTORY_CAPACITY;
        }

        if (i > 0)
            fputc(',', out);

        const StoredAnomaly *stored = &anomaly_history[index];
        const Anomaly *a = &stored->anomaly;

        fprintf(out,
            "{\"type\":\"%s\",\"pid\":%ld,"
            "\"observed_value\":%.2f,\"threshold\":%.2f,"
            "\"consecutive_samples\":%u,\"timestamp\":%lld,"
            "\"explanation\":",
            anomaly_type_name(a->type), (long)a->pid,
            a->observed_value, a->threshold,
            a->consecutive_samples, (long long)stored->timestamp);

        write_json_string(out, a->explanation);
        fputc('}', out);
    }

    fputs("]}\n", out);

    if (fclose(out) != 0) {
        free(json);
        return -1;
    }

    char header[256];

    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n",
        json_length);

    int result = -1;

    if (n > 0 && (size_t)n < sizeof(header) &&
        send_all(client, header, (size_t)n) == 0) {
        result = send_all(client, json, json_length);
    }

    free(json);
    return result;
}

static int handle_request(int client)
{
    char request[REQUEST_MAX + 1] = {0};

    if (read_request(client, request, sizeof(request)) != 0) {
        return send_response(client, 400, "Bad Request",
            "{\"error\":\"invalid or oversized request\"}\n");
    }

    char method[16] = {0};
    char path[128] = {0};
    char version[16] = {0};

    int fields = sscanf(request, "%15s %127s %15s",
                        method, path, version);

    if (fields != 3 ||
        (strcmp(version, "HTTP/1.1") != 0 &&
         strcmp(version, "HTTP/1.0") != 0)) {
        return send_response(client, 400, "Bad Request",
            "{\"error\":\"bad request line\"}\n");
    }

    if (strcmp(method, "GET") != 0) {
        return send_response(client, 405, "Method Not Allowed",
            "{\"error\":\"method not allowed\"}\n");
    }

    if (strcmp(path, "/health") == 0) {
        return send_response(client, 200, "OK",
            "{\"status\":\"ok\"}\n");
    }

    if (strcmp(path, "/api/status") == 0) {
        return send_response(client, 200, "OK",
            "{"
            "\"api\":\"connected\","
            "\"process_monitor\":\"active\","
            "\"anomaly_detection\":\"active\","
            "\"supervisor\":\"cli_only\","
            "\"dashboard_supervision\":\"not_implemented\","
            "\"recovery\":\"detection_only\","
            "\"automatic_recovery\":\"not_implemented\""
            "}\n");
    }


    if (strcmp(path, "/api/processes") == 0 ||
        strcmp(path, "/api/metrics") == 0) {
        return send_processes(client);
    }

    if (strcmp(path, "/api/anomalies") == 0) {
        return send_anomalies(client);
    }

    return send_response(client, 404, "Not Found",
        "{\"error\":\"route not found\"}\n");
}

static int open_server(unsigned short port)
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0)
        return -1;

    int reuse = 1;

    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) != 0) {
        int saved = errno;
        close(server);
        errno = saved;
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, (struct sockaddr *)&address,
             sizeof(address)) != 0 ||
        listen(server, 16) != 0) {
        int saved = errno;
        close(server);
        errno = saved;
        return -1;
    }

    return server;
}

int api_server_serve_once(unsigned short port)
{
    int server = open_server(port);
    if (server < 0)
        return -1;

    int client;

    do {
        client = accept(server, NULL, NULL);
    } while (client < 0 && errno == EINTR);

    int result = -1;

    if (client >= 0) {
        result = handle_request(client);
        close(client);
    }

    close(server);
    return result;
}

int api_server_run(unsigned short port)
{
    if (monitor_init(&process_monitor) != 0)
        return -1;

    monitor_ready = 1;
    anomaly_detector_init(&anomaly_detector, 80.0, 512.0, 3);
    anomaly_count = 0;
    anomaly_next = 0;

    int server = open_server(port);

    if (server < 0) {
        monitor_destroy(&process_monitor);
        monitor_ready = 0;
        return -1;
    }

    printf("AegisOS API listening on http://127.0.0.1:%u\n", port);
    fflush(stdout);

    for (;;) {
        int client = accept(server, NULL, NULL);

        if (client < 0) {
            if (errno == EINTR)
                continue;

            break;
        }

        handle_request(client);
        close(client);
    }

    close(server);
    monitor_destroy(&process_monitor);
    monitor_ready = 0;

    return -1;
}
