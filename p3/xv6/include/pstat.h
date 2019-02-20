#ifndef _PSTAT_H_
#define _PSTAT_H_

#include "param.h"

// Lottery Scheduler

struct pstat {
  int inuse[NPROC];   // whether slot of the process table is in use (1 or 0) 
  int tickets[NPROC]; // the number of tickets this process has
  int pid[NPROC];     // the PID of each process
  int ticks[NPROC];   // the number of time slices given (total # times scheduled)
  // increment ticks everytime this proc is selected by scheduler to run
};

#endif // _PSTAT_H_
