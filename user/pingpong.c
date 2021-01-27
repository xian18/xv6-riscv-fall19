#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int parent_pipe[2], children_pipe[2];
    int pid;
    char buffer[10];
    // read_int;
    if (pipe(parent_pipe) != 0 || pipe(children_pipe) != 0)
    {

        printf("pipe error\n");
        exit();
    }
    if ((pid = fork()) > 0)
    {
        close(parent_pipe[0]);
        close(children_pipe[1]);
        write(parent_pipe[1], "ping", 5);
        close(parent_pipe[1]);
        memset(buffer, 0, sizeof buffer);
        while (read(children_pipe[0], buffer, (sizeof buffer) - 1) > 0)
        {
            printf("%d: received %s\n", getpid(), buffer);
            memset(buffer, 0, sizeof buffer);
        }
        close(children_pipe[0]);
        wait();
        exit();
    }
    else if (pid == 0)
    {
        close(children_pipe[0]);
        close(parent_pipe[1]);
        memset(buffer, 0, sizeof buffer);
        while (read(parent_pipe[0], buffer, (sizeof buffer) - 1) > 0)
        {
            printf("%d: received %s\n", getpid(), buffer);
            memset(buffer, 0, sizeof buffer);
        }
        close(parent_pipe[0]);
        write(children_pipe[1], "pong", 5);
        close(children_pipe[1]);
        exit();
    }
    else
    {
        printf("fork error\n");
        exit();
    }

    exit();
}