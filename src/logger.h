#ifndef LOGGER_H
#define LOGGER_H

void log_recovery(int pid, int score, float cpu, const char *action);
void show_logs();

#endif