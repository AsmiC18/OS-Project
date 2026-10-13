#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define BUFFER_SIZE 5

typedef struct
{
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    sem_t empty;
    sem_t full;
    sem_t mutex;
} SharedData;

void semaphore_demo()
{
    SharedData *shared;

    shared = mmap(NULL,
                  sizeof(SharedData),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS,
                  -1,
                  0);

    if (shared == MAP_FAILED)
    {
        perror("mmap");
        return;
    }

    shared->in = 0;
    shared->out = 0;

    sem_init(&shared->empty, 1, BUFFER_SIZE);
    sem_init(&shared->full, 1, 0);
    sem_init(&shared->mutex, 1, 1);

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid == 0)
    {
        // Consumer

        for (int i = 0; i < 5; i++)
        {
            sem_wait(&shared->full);
            sem_wait(&shared->mutex);

            int item = shared->buffer[shared->out];

            shared->out =
                (shared->out + 1) % BUFFER_SIZE;

            printf("Consumer consumed: %d\n", item);

            sem_post(&shared->mutex);
            sem_post(&shared->empty);

            sleep(1);
        }
    }
    else
    {
        // Producer

        for (int i = 1; i <= 5; i++)
        {
            sem_wait(&shared->empty);
            sem_wait(&shared->mutex);

            shared->buffer[shared->in] = i;

            shared->in =
                (shared->in + 1) % BUFFER_SIZE;

            printf("Producer produced: %d\n", i);

            sem_post(&shared->mutex);
            sem_post(&shared->full);

            sleep(1);
        }

        wait(NULL);

        sem_destroy(&shared->empty);
        sem_destroy(&shared->full);
        sem_destroy(&shared->mutex);

        munmap(shared, sizeof(SharedData));
    }
}