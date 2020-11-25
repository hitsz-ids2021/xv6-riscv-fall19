#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fcntl.h"

# define MAX_CMD 10

# define ORDINARY 0
# define REDIRECT 1
# define PIPE 2
# define BLANK 3

struct cmd {
    int argc;
    char *args[MAXARG];
    char *input;
    char *output;
};

int ParseCmd(struct cmd *cmd, int cmd_size, char *buf, int buf_size);
int CharType(char x);
void RunCmd(struct cmd *cmd, int cmd_size, int index);
void ExecCmd(struct cmd cmd);

int 
main(void) {
    static char buf[100];
    static struct cmd cmd[MAX_CMD];
    int cmd_size = 0;

    // read input
    while((cmd_size = ParseCmd(cmd, MAX_CMD, buf, sizeof buf)) >= 0) {
        if(cmd_size == 0) {
            continue;
        }
        RunCmd(cmd, cmd_size, 0);
    }
    exit(0);
}

void 
RunCmd(struct cmd *cmd, int size, int index) {
    if(size == 1) {
        if(fork() == 0) {
            ExecCmd(cmd[index]);
        }
        wait(0);
        return;
    }

    int p[2];
    pipe(p);
    
    // output -> pipe
    if(fork() == 0) {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        ExecCmd(cmd[index]);
    }

    // pipe -> input
    if(fork() == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        RunCmd(cmd, size - 1, index + 1);
        exit(0);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    return;
}

void 
ExecCmd(struct cmd cmd) {
    if(cmd.input) {
        close(0);
        open(cmd.input, O_RDONLY);
    }
    if(cmd.output) {
        close(1);
        open(cmd.output, O_WRONLY | O_CREATE);
    }
    exec(cmd.args[0], cmd.args);
}

// deal with input strings
int 
ParseCmd(struct cmd *cmd, int cmd_size, char *buf, int buf_size) {
    int real_buf_size = 0;
    int real_cmd_size = 0;

    printf("@ ");
    gets(buf, buf_size);
    real_buf_size = strlen(buf);
    if(real_buf_size < 1) {
        return -1;
    }

    if(real_buf_size > 1) {
        // replace '\n' with 0
        buf[real_buf_size-1] = 0;
        real_cmd_size = 1;

        char *x = buf;
        while(*x) {
            int pipe_flag = 0;
            int cmd_index = real_cmd_size - 1;

            // init command
            cmd[cmd_index].argc = 0;
            cmd[cmd_index].input = 0;
            cmd[cmd_index].output = 0;
            for(int i = 0; i < MAXARG; i++){
                cmd[cmd_index].args[i] = 0;
            }

            // get args util meeting a pipe
            while(*x && !pipe_flag) {
                switch(*x) {
                    case ' ':
                        while(*x && *x == ' ') *x++ = 0;
                        break;
                    case '>':
                        x++;
                        while(*x && *x == ' ') *x++ = 0;
                        cmd[cmd_index].output = x;
                        while(*x && CharType(*x) == ORDINARY) x++;
                        break;
                    case '<':
                        x++;
                        while(*x && *x == ' ') *x++ = 0;
                        cmd[cmd_index].input = x;
                        while(*x && CharType(*x) == ORDINARY) x++;
                        break;
                    case '|':
                        x++;
                        real_cmd_size++;
                        pipe_flag = 1;
                        break;
                    // oridinary character
                    default:
                        cmd[cmd_index].argc++;
                        cmd[cmd_index].args[cmd[cmd_index].argc-1] = x;
                        while(*x && CharType(*x) == ORDINARY) x++;
                        break;
                }
            }
        }
    }

    // ignore invalid pipe
    if((real_cmd_size > 1) && cmd[real_cmd_size-1].argc < 1) {
        return real_cmd_size - 1;
    } else {
        return real_cmd_size;
    }
}

int
CharType(char x) {
    if(x == '<' || x == '>') {
        return REDIRECT;
    } else if (x == '|') {
        return PIPE;
    } else if (x == ' '){
        return BLANK;
    } else {
        return ORDINARY;
    }
}