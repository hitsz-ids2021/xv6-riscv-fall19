#include "user/user.h"
#include "kernel/types.h"

void main(int argc, char *argv[]) {
    
    int sp[2];
    int rp[2];
    int pid;
    pipe(sp);
    pipe(rp);
    char buf[10];
    pid = fork();

// pid = 0,child; pid>0,parent
    if(pid==0){
        read(sp[0],buf,5);
        if (strcmp(buf,"ping") == 0)
        {
            printf("%d: received ping\n",getpid());
            close(rp[0]);
            write(rp[1],"pong",5);
            close(rp[1]);
            exit();
        }
    } else if (pid>0){
        close(sp[0]);
        write(sp[1],"ping",5);
        close(sp[1]);
        read(rp[0],buf,5);
        if (strcmp(buf,"pong") == 0){
            printf("%d: received pong\n",getpid());
            exit();
        }
    } else{
        printf("fork error!\n");
        exit();
    }
}