#include "../kernel/types.h"
#include "../kernel/memlayout.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    uint64 phys_size = PHYSTOP - KERNBASE;
    int sz = (phys_size / 3) * 2;
    char *p = sbrk(sz);
    if (p == (char *) 0xffffffffffffffffL) {
        printf("error: sbrk(%d) failed\n", sz);
        exit(-1);
    }
    for (char *q = p; q < p + sz; q += 4096) {
        *(int *) q = getpid();
    }
    int pid = fork();
    if (pid < 0) {
        printf("error: fork() failed\n");
        exit(-1);
    }
    if (pid == 0) {
        printf("child process successfully forked\n");
        exit(0);
    }
    else{
        wait(0);
        printf("parent process successfully forked\n");
    }
    if (sbrk(-sz) == (char *) 0xffffffffffffffffL) {
        printf("error: sbrk(-%d) failed\n", sz);
        exit(-1);
    }
    printf("test OK\n");
    exit(0);
}

