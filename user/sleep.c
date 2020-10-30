#include "user/user.h"
#include "kernel/types.h"


int main(int argc, char *argv[]){
    int sleep_sec;
    if(argc <2) {
        printf("Usage: sleep seconds\n");
        exit();
    }
    sleep_sec = atoi(argv[1]);
	if (sleep_sec > 0){
        printf("(nothing happened for a little while)\n");
		sleep(sleep_sec);
	} else {
		printf("Invalid interval %s\n", argv[1]);
	}
	exit();
}
