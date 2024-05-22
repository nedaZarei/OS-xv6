#include "user.h"
#include "../kernel/memlayout.h"
#include "../kernel/topStruct.h"


int main(int argc, char **argv){
    struct top top_struct;
    top((uint64) &top_struct);

    printf("Total processes: %d  Running processes: %d  Sleeping processes: %d",
           top_struct.total_process, top_struct.running_process, top_struct.sleeping_process);

    printf("\nuptime : %d\n",top_struct.uptime);

    printf("\nPID   PPID   STATE       NAME    TIME   CPU-USAGE\n");
    for (int i = 0; i < top_struct.total_process; i++) {
        printf("%d    %d    %s   %s      %d      %d%%\n",
                top_struct.p_list[i].pid, top_struct.p_list[i].ppid,
                top_struct.p_list[i].state, top_struct.p_list[i].name,
                top_struct.p_list[i].ctime, (int) (top_struct.p_list[i].cpu_usage *100.0));

        printf("running time: %d\n", top_struct.p_list[i].rtime);
    }
    exit(0);
}
