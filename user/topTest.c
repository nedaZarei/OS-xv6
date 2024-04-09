#include "user.h"
#include "../kernel/memlayout.h"
#include "../kernel/topStruct.h"


int main(int argc, char **argv){
    struct top top_struct;
    top((uint64) &top_struct);

    printf("Total processes: %d  Running processes: %d  Sleeping processes: %d",
           top_struct.total_process, top_struct.running_process, top_struct.sleeping_process);

    printf("\nCPU Size : %d \nTicks : %ld\n", (int) PHYSTOP, top_struct.uptime);

    printf("\nPID   PPID   STATE       NAME");
    for (int i = 0; i < top_struct.total_process; i++) {
        printf("\n %d    %d    %s   %s",
                top_struct.p_list[i].pid, top_struct.p_list[i].ppid,
                top_struct.p_list[i].state, top_struct.p_list[i].name);
    }
    exit(0);
}
