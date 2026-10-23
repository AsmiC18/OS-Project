
#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures;

static void check(int condition, const char *message)
{
    printf("%s: %s\n", condition ? "PASS" : "FAIL", message);

    if (!condition)
        failures++;
}

int main(void)
{
    AegisProcessInfo info;

    memset(&info, 0, sizeof(info));

    check(process_read(getpid(), &info) == 0,
          "reads the current process");

    if (info.pid == getpid()) {
        check(info.name[0] != '\0',
              "process name is populated");
        check(info.state != '\0',
              "process state is populated");
        check(info.total_ticks == info.user_ticks + info.system_ticks,
              "total CPU ticks equal user plus system ticks");
        check(info.resident_pages >= 0,
              "resident page count is non-negative");
    }

    errno = 0;
    check(process_read(-1, &info) == -1 && errno == EINVAL,
          "rejects a negative PID");

    errno = 0;
    check(process_read(0, &info) == -1 && errno == EINVAL,
          "rejects PID zero");

    check(process_read(2147483647, &info) == -1,
          "rejects a nonexistent process");

    check(process_read(getpid(), NULL) == -1,
          "rejects a null output pointer");

    printf("\nProcess tests: %d failure(s)\n", failures);

    return failures == 0 ? 0 : 1;
}
