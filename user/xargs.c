#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int i,j;
    int next_arg;
    char args[MAXARG][32];
    char *p[MAXARG];
    char buf;

    if(argc < 2) {
        fprintf(2, "usage: xargs <cmd> ...\n");
        exit();
    }
    while(1) {
        i=0;
        next_arg=0;
        memset(args,0,MAXARG*32);

        for(j=1;j<argc;j++){
            strcpy(args[i++],argv[j]);
        }
        j=0;
        while (i < MAXARG-1) { //从标准输入读
            if ( read(0,&buf,1) <= 0) {  //buf空,ctrl+d
                wait();
                exit();
            }
            if ( buf == '\n') {
                break;
            }
            if (buf == ' ') {
                if (next_arg) {   //执行xarg后，遇到空格，如果不是所有参数前的空格，则表示到下一个参数。
                    i++;
                    j=0;
                    next_arg=0;
                }
                continue;
            }
            args[i][j++] = buf;
            next_arg = 1;
        }
        for (i=0;i<MAXARG-1;i++){
            p[i] = args[i];
        }
        p[MAXARG-1] = 0;

        if (fork()==0) {   //创建子进程执行命令
            exec(argv[1],p);
            exit();
        }
        
    }
    exit();
}