#define NPROC        64  // maximum number of processes
struct proc_info{
    char name[16];
    int pid;
    int ppid;
    char state[];
};
struct top{
    long uptime;
    int total_process; //total num of processes
    int running_process; //total num of running processes
    int sleeping_process; //total num of sleeping processes
    // a list of proc info including:
    // pid, parent pid, state of proc(running,sleeping,runnable,unused), name of proc
    struct proc_info p_list[NPROC];
};