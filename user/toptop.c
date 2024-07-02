#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/memlayout.h"
#include "../kernel/topStruct.h"


void clear_screen() {
    printf("\033[H\033[2J"); //clear screen and move cursor to top-left corner
}

int main(int argc, char **argv) {
    struct top top_struct;

    while (1) {
        top((uint64) &top_struct);

        clear_screen();
        printf("Total processes: %d  Running processes: %d  Sleeping processes: %d\n",
               top_struct.total_process, top_struct.running_process, top_struct.sleeping_process);

        printf("Uptime : %l seconds\n", top_struct.uptime);

        printf("\nPID   PPID   STATE       NAME    TIME(s)   %CPU   %MEM\n");
        for (int i = 0; i < top_struct.total_process; i++) {
            printf(" %d    %d    %s     %s      %d       %d%%      %d%%\n",
                   top_struct.p_list[i].pid, top_struct.p_list[i].ppid,
                   top_struct.p_list[i].state, top_struct.p_list[i].name,
                   top_struct.p_list[i].time, (int) (top_struct.p_list[i].cpu_usage * 100.0)/*,
                   top_struct.p_list[i].mem_usage_percent*/ );
        }
        printf("----------------------------------------------------------------\n");
        printf("MEMORY STATISTICS:\n");
        printf("total : %d bytes\n", top_struct.total_mem);
        printf("used : %d bytes\n", top_struct.used_mem);
        printf("free : %d bytes\n", top_struct.free_mem);
        printf("----------------------------------------------------------------\n");
        //sleep for a short period before refreshing
        sleep(2);
    }
}
