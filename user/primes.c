#include "kernel/types.h"
#include "user/user.h"
#define EXIT_NUM 35
int primes(int *read_pipes)
{
    int write_pipes[2];
    int prime_num, tmp_num;
    close(read_pipes[1]);
    if (read(read_pipes[0], &prime_num, sizeof prime_num) == 0)
    {
        exit();
    }
    printf("prime %d\n", prime_num);
    if (prime_num == EXIT_NUM)
    {
        close(read_pipes[0]);
        exit();
    }
    pipe(write_pipes);
    if (fork() > 0)
    {
        close(write_pipes[0]);
        while (read(read_pipes[0], &tmp_num, sizeof tmp_num) > 0)
        {
            if (tmp_num % prime_num)
            {
                write(write_pipes[1], &tmp_num, sizeof tmp_num);
            }
        }
        close(read_pipes[0]);
        exit();
    }
    else
    {
        primes(write_pipes);
    }
    exit();
    return 0;
}
int main(int argc, char *argv[])
{
    int i;
    int write_pipes[2];
    pipe(write_pipes);
    if (fork() > 0)
    {
        close(write_pipes[0]);
        for (i = 2; i <= EXIT_NUM; i++)
        {
            write(write_pipes[1], &i, sizeof i);
        }
        close(write_pipes[1]);
        exit();
    }
    else
    {
        primes(write_pipes);
    }
    exit();
    return 0;
}

#undef EXIT_NUM