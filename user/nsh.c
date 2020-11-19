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
int CheakRedirect(struct cmd *cmd, int cmd_size);
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
        // error redirect
        if(!CheakRedirect(cmd, cmd_size)) {
            fprintf(2, "bad redirect!\n");
            continue;
        }
        RunCmd(cmd, cmd_size, 0);
    }
    // fprintf(2, "main will Never arrive here.\n");
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
    
    if(fork() == 0) {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        ExecCmd(cmd[index]);
    }

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
            int cmd_index = real_cmd_size - 1;
            // replace blank with 0
            while(*x && *x == ' ') {
                *x = 0;
                x++;
            }

            // init command
            cmd[cmd_index].argc = 0;
            cmd[cmd_index].input = 0;
            cmd[cmd_index].output = 0;
            for(int i = 0; i < MAXARG; i++){
                cmd[cmd_index].args[i] = 0;
            }

            // get the pointer to command
            while(*x && CharType(*x) == ORDINARY) {
                cmd[cmd_index].argc++;
                cmd[cmd_index].args[cmd[cmd_index].argc-1] = x;
                // skip
                while(*x && CharType(*x) == ORDINARY) {
                    x++;
                }
                // replace blank with 0
                while(*x && CharType(*x) == BLANK) {
                    *x = 0;
                    x++;
                }
            }

            // catch redirect signal
            while(CharType(*x) == REDIRECT) {
                char redirect = *x++;
                while(*x && CharType(*x) == BLANK) {
                    x++;
                }
                // distinguish input and output
                if(redirect == '<') {
                    cmd[cmd_index].input = x;
                } else {
                    cmd[cmd_index].output = x;
                }
                // skip
                while(*x && CharType(*x) == ORDINARY) {
                    x++;
                }
                // replace blank with 0
                while(*x && CharType(*x) == BLANK) {
                    *x = 0;
                    x++;
                }
            }

            // catch pipe signal
            if(CharType(*x) == PIPE){
                x++;
                if(cmd[cmd_index].argc > 0) {
                    real_cmd_size++;
                } else {
                    fprintf(2, "bad pipe\n");
                    return 0;
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
CheakRedirect(struct cmd *cmd, int cmd_size) {
    if(cmd_size <= 1) {
        return 1;
    }
    for(int i = 0; i < cmd_size; i++) {
        if(i == 0 && cmd[i].output != 0) {
            return 0;
        }
        if(i == cmd_size-1 && cmd[i].input != 0) {
            return 0;
        }
        if((i != 0 && i != cmd_size-1) && (cmd[i].input != 0 || cmd[i].output != 0)) {
            return 0;
        }
    }
    return 1;
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
