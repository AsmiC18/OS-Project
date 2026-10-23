
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char *path = "rotation_test.jsonl";
    const char *backup = "rotation_test.jsonl.1";

    remove(path);
    remove(backup);

    if (logger_open(path, 100) != 0) {
        perror("logger_open");
        return 1;
    }

    for (int i = 0; i < 5; ++i) {
        if (logger_event(LOG_LEVEL_INFO, "rotation_test",
                         "This message triggers log rotation") != 0) {
            perror("logger_event");
            logger_close();
            return 1;
        }
    }

    if (logger_close() != 0) {
        perror("logger_close");
        return 1;
    }

    FILE *file = fopen(backup, "r");
    if (!file) {
        fprintf(stderr, "Rotation backup file was not created.\n");
        return 1;
    }
    fclose(file);

    puts("Logger rotation test passed.");
    remove(path);
    remove(backup);
    return 0;
}
