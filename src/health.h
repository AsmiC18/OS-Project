
#ifndef HEALTH_H
#define HEALTH_H

int calculate_health(float cpu, float memory, char state, int priority);
void show_health(void);
int measure_process_health(int pid, float *cpu_out, float *memory_out);

#endif
