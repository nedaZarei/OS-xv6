#include "../kernel/types.h"
#include "../kernel/memlayout.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    uint64 phys_size = PHYSTOP - KERNBASE;
    int sz = (phys_size / 3) * 2;
    char *p = sbrk(sz);
    //allocates sz bytes of memory using the sbrk() system call
    //sbrk() adjusts the heap size of the process,allowing it to allocate more memory dynamically
    if (p == (char *) 0xffffffffffffffffL) {
        printf("error: sbrk(%d) failed\n", sz);
        exit(-1);
    }
    for (char *q = p; q < p + sz; q += 4096) {
        //initializing the allocated memory (p) by writing the process id (getpid()) into each page
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
        //decreasing the process's heap size by sz bytes, freeing the previously allocated memory
        printf("error: sbrk(-%d) failed\n", sz);
        exit(-1);
    }
    printf("test OK\n");
    exit(0);
}

