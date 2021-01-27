#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#define MAXARGS 10
int main(int argc, char *argv[])
{
    char *args[MAXARGS + 1];
    char buf[1024], *p, c;
    char whitespace[] = " \t\r\n\v";
    int i, t;

    for (;;)
    {
        for (i = 1; i < argc; i++)
        {
            args[i - 1] = argv[i];
        }
        memset(buf, 0, sizeof buf);
        p = buf;
        for (i = argc - 1; i < MAXARGS; i++)
        {
            while ((t = read(0, &c, 1)) > 0 && strchr(whitespace, c))
                ;
            args[i] = p;
            *p = c;
            p++;
            while ((t = read(0, &c, 1)) > 0 && !strchr(whitespace, c))
            {
                *p = c;
                p++;
            }
            if (t == 0)
            {
                exit();
            }
            else if (c == '\n')
            {
                break;
            }
            else
            {
                p++;
            }
        }

        if (fork() == 0)
        {
            exec(args[0], args);
            exit();
        }
        wait();
    }
}