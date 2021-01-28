#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"

#define REDIR_INPUT 0x1
#define REDIR_OUTPUT 0x2
#define PIPE_CMD 0x1
struct exec_cmd
{
    char *CMD;
    char *CMD_ARG[MAXARG];
    char *REDIR_INPUT_PATH;
    char *REDIR_OUTPUT_PATH;
    char type;
};
typedef struct cmd_t
{
    struct exec_cmd L_CMD, R_CMD;
    int pipes[2];
    char type;
} cmd_t;
char whitespace[] = " \t\r\v";

void panic(const char *);
void parseCMD(char *buf, cmd_t *cmd);
void execCMD(cmd_t *cmd);
void getCMD(char *buf, int len);
void printECMD(struct exec_cmd *);

int main(int argc, char *argv[])
{
    cmd_t cmd;
    char buf[1024];
    for (;;)
    {
        getCMD(buf, 1024);
        parseCMD(buf, &cmd);
        execCMD(&cmd);
    }
}
void getCMD(char *buf, int len)
{
    char c, *p = buf;
    memset(buf, 0, len);
    fprintf(1, "@ ");
    while (read(0, &c, 1) > 0)
    {
        *p = c;
        p++;
        if (p >= buf + len)
        {
            panic("cmd too long\n");
        }
        else if (c == '\n')
        {
            return;
        }
    }
    exit(0);
}

void __exec_CMD(struct exec_cmd *ecmd);
void execCMD(cmd_t *cmd)
{
    if (!cmd->type & PIPE_CMD)
    {
        if (fork() == 0)
        {
            __exec_CMD(&cmd->L_CMD);
        }
        wait(0);
        return;
    }
    pipe(cmd->pipes);
    if (fork() == 0) //LCMD
    {
        close(1);
        dup(cmd->pipes[1]);
        close(cmd->pipes[0]);
        close(cmd->pipes[1]);
        __exec_CMD(&cmd->L_CMD);
    }
    if (fork() == 0) //LCMD
    {
        close(0);
        dup(cmd->pipes[0]);
        close(cmd->pipes[0]);
        close(cmd->pipes[1]);
        __exec_CMD(&cmd->R_CMD);
    }
    close(cmd->pipes[0]);
    close(cmd->pipes[1]);
    wait(0);
    wait(0);
}

void __exec_CMD(struct exec_cmd *ecmd)
{
    int result;
    if (ecmd->type & REDIR_INPUT)
    {
        close(0);
        open(ecmd->REDIR_INPUT_PATH, O_RDONLY);
    }
    if (ecmd->type & REDIR_OUTPUT)
    {
        close(1);
        open(ecmd->REDIR_OUTPUT_PATH, O_CREATE | O_WRONLY);
    }
    result = exec(ecmd->CMD_ARG[0], ecmd->CMD_ARG);
    if (ecmd->type & REDIR_INPUT)
    {
        close(0);
    }
    if (ecmd->type & REDIR_OUTPUT)
    {
        close(1);
    }
    exit(result);
}

char *__parse_CMD(char *, cmd_t *, struct exec_cmd *);
void parseCMD(char *buf, cmd_t *cmd)
{
    char *p = buf;
    memset(cmd, 0, sizeof(cmd_t));
    p = __parse_CMD(p, cmd, &cmd->L_CMD);
    //printECMD(&cmd->L_CMD);
    if (*p == '|' || cmd->type & PIPE_CMD)
    {
        p = __parse_CMD(p, cmd, &cmd->R_CMD);
    }
    return;
}
char *__parse_CMD(char *p, cmd_t *cmd, struct exec_cmd *ecmd)
{
    int argc;
    for (; strchr(whitespace, *p); p++)
        ;

    //R_CMD
    ecmd->CMD = p;
    ecmd->CMD_ARG[0] = p;
    for (; !strchr(whitespace, *p) && *p != '\n'; p++)
        ;
    if (*p == '\n')
    {
        *p = 0;
        p++;
        return p;
    }
    *p = 0;
    p++;
    for (argc = 1; argc < MAXARG || cmd->type & PIPE_CMD;)
    {
        for (; strchr(whitespace, *p); p++)
            ;
        switch (*p)
        {
        case '>': //R_REDIR_OUTPUT
            ecmd->type |= REDIR_OUTPUT;
            for (p++; strchr(whitespace, *p); p++)
                ;
            ecmd->REDIR_OUTPUT_PATH = p;
            for (; !strchr(whitespace, *p) && *p != '\n'; p++)
                ;
            if (*p == '\n')
            {
                *p = 0;
                p++;
                return p;
            }
            *p = 0;
            p++;
            break;
        case '<': //R_REDIR_INPUT
            ecmd->type |= REDIR_INPUT;
            for (p++; strchr(whitespace, *p); p++)
                ;
            ecmd->REDIR_INPUT_PATH = p;
            for (; !strchr(whitespace, *p) && *p != '\n'; p++)
                ;
            if (*p == '\n')
            {
                *p = 0;
                p++;
                return p;
            }
            *p = 0;
            p++;
            break;
        case '|': //PIPE_CMD
            cmd->type |= PIPE_CMD;
        case '\n':
            p++;
            return p;
        default: // ARGS
            ecmd->CMD_ARG[argc] = p;
            for (p++; !strchr(whitespace, *p) && *p != '\n'; p++)
                ;
            if (*p == '\n')
            {
                *p = 0;
                p++;
                return p;
            }
            *p = 0;
            p++;
            argc++;
        }
    }
    return p;
}
void panic(const char *str)
{
    fprintf(2, "%s", str);
    exit(-1);
}
void printECMD(struct exec_cmd *ecmd)
{
    char **p = ecmd->CMD_ARG;
    fprintf(2, "CMD :");
    for (; *p; p++)
        fprintf(2, "%s ", *p);
    fprintf(2, "\n");
    if (ecmd->type & REDIR_INPUT)
    {
        fprintf(2, "REDIR INPUT: %s\n", ecmd->REDIR_INPUT_PATH);
    }
    if (ecmd->type & REDIR_OUTPUT)
    {
        fprintf(2, "REDIR OUTPUT: %s\n", ecmd->REDIR_OUTPUT_PATH);
    }
    return;
}

#undef L_REDIR_INPUT
#undef L_REDIR_OUTPUT
#undef PIPE_CMD
#undef R_REDIR_INPUT
#undef R_REDIR_OUTPUT
#undef MAX_CMD_LEN