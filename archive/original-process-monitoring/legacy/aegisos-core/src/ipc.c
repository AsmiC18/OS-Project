#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

void pipe_communication()
{
    int parent_to_child[2];
    int child_to_parent[2];

    char message1[] = "Hello Child";
    char message2[] = "Hello Parent";

    char buffer[100];

    pipe(parent_to_child);
    pipe(child_to_parent);

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid > 0)
    {
        // Parent process

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        // Parent sends message
        write(parent_to_child[1], message1, strlen(message1) + 1);

        printf("Parent sent: %s\n", message1);

        // Parent receives reply
        read(child_to_parent[0], buffer, sizeof(buffer));

        printf("Parent received: %s\n", buffer);

        close(parent_to_child[1]);
        close(child_to_parent[0]);

        wait(NULL);
    }
    else
    {
        // Child process

        close(parent_to_child[1]);
        close(child_to_parent[0]);

        // Child receives message
        read(parent_to_child[0], buffer, sizeof(buffer));

        printf("Child received: %s\n", buffer);

        // Child sends reply
        write(child_to_parent[1], message2, strlen(message2) + 1);

        printf("Child sent: %s\n", message2);

        close(parent_to_child[0]);
        close(child_to_parent[1]);
    }
}