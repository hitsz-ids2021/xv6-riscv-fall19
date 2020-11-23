#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXARGS 10  // 命令行语句的参数个数最大值
#define MAXWORD 30  // 参数的长度最大值
#define MAXLINE 100

char args[MAXARGS][MAXWORD];
char whitespace[] = " \t\r\n\v";

int getcmd(char *buf, int nbuf);
void setargs(char *cmd, char* argv[],int* argc);
void runcmd(char*argv[], int argc);
void execPipe(char*argv[], int argc);

// 参考sh.c中的getcmd函数
int getcmd(char *buf, int nbuf){
    fprintf(2, "@ ");  // 用”@”代替”$” 作为命令行输入提示符
    memset(buf, 0, nbuf);  // 清空buf数组
    gets(buf, nbuf);  // 从标准输入读入nbuf字符，存入buf
    if(buf[0] == 0) // EOF
        return -1;
    return 0;
}

void setargs(char *cmd, char* argv[],int* argc)
{
    // 让argv的每一个元素都指向args的每一行
    for(int i=0; i<MAXARGS; i++){
        argv[i] = &args[i][0];
    }
    int i; // 表示第i个参数
    int j;  // 表示参数中的第j个char
    for (i=0, j=0; cmd[j]!='\n' && cmd[j]!='\0'; j++)
    {
        // 每一轮循环都是找到输入的命令中的一个word，比如 echo hi ,就是先找到echo，再找到hi
        // 让argv[i]分别指向他们的开头，并且将echo，hi后面的空格设为\0
        // 跳过之前的空格
        while (strchr(whitespace, cmd[j])){  // strchr返回的是在whitespace字符串中第一次出现cmd[j]的位置，未找到该字符则返回null
            j++;
        }
        // j指向有用的参数的开头
        argv[i++] = cmd + j;
        // 找下一个空格
        while (strchr(whitespace,cmd[j])==0){
            j++;
        }
        // 在参数的末尾加上\0
        cmd[j]='\0';
    }
    argv[i] = 0;  // 执行exec()的时候，最后一个参数argv[size-1]必须为0，否则将执行失败。
    *argc = i;
}

void runcmd(char*argv[], int argc)
{
    for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "|")){
            // 如果遇到 | 即pipe，至少说明后面还有一个命令要执行
            execPipe(argv,argc);
        }
    }
    // 此时是仅处理一个命令：现在判断argv[1]开始，后面有没有> 
    for(int i=1; i<argc; i++){
        // 如果遇到 > ，说明需要执行输出重定向
        if(!strcmp(argv[i], ">")){
            // 关闭stdout
            close(1);
            // 把输出重定向到后面给出的文件名对应的文件里
            open(argv[i+1], O_CREATE|O_WRONLY);  // 以只写的方式打开文件，若文件不存在则创建
            argv[i] = 0;  // 执行exec()的时候，最后一个参数argv[size-1]必须为0，否则将执行失败。
        }
        // 如果遇到< ,需要执行输入重定向
        if(!strcmp(argv[i],"<")){
            // 关闭stdin
            close(0);
            // 把输入重定向到后面给出的文件名对应的文件里
            open(argv[i+1],O_RDONLY);  // 以只读的方式打开文件
            argv[i] = 0;  // 执行exec()的时候，最后一个参数argv[size-1]必须为0，否则将执行失败。
        }
    }
    exec(argv[0], argv);  // argv[0]必须是该命令本身
}

// 参考sh.c中PIPE的实现，第100行
void execPipe(char*argv[], int argc){
    int i=0;
    // 首先找到命令中的"|",然后把他换成'\0'
    // 从前到后，找到第一个就停止，后面都递归调用
    for(; i<argc; i++){
        if(!strcmp(argv[i],"|")){
            argv[i] = 0;
            break;
        }
    }
    // 先考虑最简单的情况：cat file | wc
    int fd[2];
    pipe(fd);
    if(fork() == 0){
        // 子进程 
        // 执行左边的命令
        close(1);  // 关闭标准输出
        dup(fd[1]);
        close(fd[0]);
        close(fd[1]);
        runcmd(argv, i);
    }else{
        // 父进程 
        // 执行右边的命令
        close(0);  // 关闭标准输入
        dup(fd[0]);
        close(fd[0]);
        close(fd[1]);
        runcmd(argv+i+1,argc-i-1);
    }
}


// 参考sh.c中的main函数
int main(void){
    static char buf[100];
    // Read and run input commands.
    while(getcmd(buf, sizeof(buf)) >= 0){
        if(fork() == 0){
            char* argv[MAXARGS];
            int argc = -1;
            setargs(buf, argv, &argc);
            runcmd(argv, argc);
        }
        wait(0);
    }
    exit(0);
}
