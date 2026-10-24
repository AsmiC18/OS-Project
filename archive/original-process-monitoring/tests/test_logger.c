
#include "logger.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *path = "test_logger_output.jsonl";
    FILE *file;

    if (logger_open(path, 0) != 0) {
        perror("logger_open");
        return 1;
    }

    if (logger_event(LOG_LEVEL_INFO, "test_event",
                     "Logger is working") != 0) {
        perror("logger_event");
        logger_close();
        return 1;
    }

    if (logger_event(LOG_LEVEL_WARNING, "escaped",
                     "quoted: \"hello\"") != 0) {
        perror("logger_event");
        logger_close();
        return 1;
    }

    if (logger_close() != 0) {
        perror("logger_close");
        return 1;
    }

    file = fopen(path, "r");
    if (!file) {
        perror("fopen");
        return 1;
    }

    char contents[2048];
    size_t count = fread(contents, 1, sizeof(contents) - 1, file);
    contents[count] = '\0';
    fclose(file);

    if (!strstr(contents, "\"level\":\"INFO\"") ||
        !strstr(contents, "\"event\":\"test_event\"") ||
        !strstr(contents, "Logger is working") ||
        !strstr(contents, "\\\"hello\\\"")) {
        fprintf(stderr, "Logger output validation failed:\n%s\n", contents);
        return 1;
    }

    puts("Logger tests passed.");
    remove(path);
    return 0;
}
