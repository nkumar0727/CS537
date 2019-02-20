#include "types.h" 
#include "user.h"
#include "pstat.h"

#define STDOUT 1
#define PROCS 3

/*
 * "Data Science"
 *
 * Run this until the number of ticks for each process
 * do not change; then kill QEMU. Copy paste terminal output
 * to file, parse file, make graph.
 */

int
main(int argc, char* argv[]) {
  // create three procs with 3:2:1 ticket ratio
  // monitor their ticks per time slice
  int parentPid = getpid();
  if(settickets(1000) == -1) {
    printf(STDOUT, "parent proc failed to settickets\n");
    exit();
  }
  struct pstat ps;
  if(getpinfo(&ps) < 0) {
    printf(STDOUT, "error w/ getpinfo\n");
    exit();
  }
  int pid;
  uint ticks_start, ticks_curr;
  int pidtbl[PROCS];
  int ptix[PROCS] = {10, 20, 30};
  for(int i = 0; i < PROCS; ++i) {
    pid = pidtbl[i] = fork();
    if(pidtbl[i] < 0) { // fork failed
      printf(STDOUT, "proc[%d] has failed to fork\n", parentPid);
      exit();
    }
    else if(pidtbl[i] == 0) { // child proc
      int rc = settickets(ptix[i]);
      if(rc == -1) {
        printf(STDOUT, "child proc[%d]; settickets failed\n", pidtbl[i]);
        exit();
      }
      // spin...
      for(int x = 0; x < 100000000; ++x) {
        x = x; x = x; x = x;
      }
      break;
    }
    // parent proc    
  }
  if(pid > 0) { // parent proc
    if(getpinfo(&ps) < 0) {
      printf(STDOUT, "error w/ getpinfo\n");
      exit();
    }
    // reference    
    ticks_start = uptime();
    for(;;) {
      ticks_curr = uptime();
      if(ticks_curr - ticks_start > 2) { // every other tick is good balance
        if(getpinfo(&ps) < 0) {
          printf(STDOUT, "error w/ getpinfo\n");
          exit();
        }
        printf(STDOUT, "(%d:%d:%d),%d,%d,%d,%d\n", ps.tickets[pidtbl[0]-1], ps.tickets[pidtbl[1]-1], ps.tickets[pidtbl[2]-1],ticks_curr, ps.ticks[pidtbl[0]-1], 
          ps.ticks[pidtbl[1]-1], ps.ticks[pidtbl[2]-1]);
        ticks_start = ticks_curr;
      }
    }
  }
  exit();
}
