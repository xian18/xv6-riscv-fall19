#include "kernel/types.h"
#include "user/user.h"
// #define SF_OPTIONAL
int main(int argc, char *argv[])
{
    int time;

    if (argc <= 1)
    {
        printf("usage sleep time\n");
        exit();
    }

    time = atoi(argv[1]);
#ifdef SF_OPTIONAL
    for (; time; time--)
    {
        uptime();
        printf("still %d\n", time);
    }
#else
    sleep(time);
#endif
    printf("sleep exited\n");
    exit();
}

#undef SF_OPTIONAL