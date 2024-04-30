#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

#define MAX_HISTORY 16
// returns 0 if succeeded, 1 if no history in the historyId given, 2 if illegal history id
uint64 sys_history(void) {
    char *buffer;
    int historyId;
    //get the address of the buffer
    argaddr(0, (uint64 *)&buffer);
    //get the historyId
    argint(1, &historyId);

    //fetch the history command string from user space
    char cmdFromHistory[MAX_HISTORY];
    if (fetchstr((uint64)buffer, cmdFromHistory, MAX_HISTORY) < 0)
        return -1; //if fetchstr fails

    //call the history function with the fetched command
    return history(cmdFromHistory, historyId);
}

// returns 0 if succeeded, -1 if unsuccessful
uint64 sys_top(void) {
    uint64 top_struct;
    argaddr(0, &top_struct);
    return top(top_struct);
}
