#include "user/user.h"
#include "kernel/types.h"

void main(){
    int pid;
    pid = fork();
    int p[2];
    int n=2;
    int cur=2;

    // if (pid == 0){

    // } else if(pid > 0){
    //     printf("prime: 2\n");
    //     pipe(p);
    //     if (++n % 2==0)
    //     {
    //         close(p[0]);
    //         write(p[1],++n,5);
    //         close(p[1]);
    //         fork();
    //     }
    // } else {
    //     printf("fork error!\n");
    //     exit();
    // }
    
    
}

int fetch(){
    
}