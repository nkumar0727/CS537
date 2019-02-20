# cs537-p3
Parallel zip and xv6 virtual memory -- Contributors: Nikhil Kumar, Andrew Briggs

## GIT REFERENCE 
* type 'git status' to see if other person made any changes
* type 'git log' to see the every commit made; the most recent might not be yours, so make sure to pull first if that's the case
* Pull any changes that other person may have made
  * type 'git pull origin master'
* Make your own changes
* Push your changes so I can see, pull, make my changes, push, etc
  * type 'git add -A'
  * type 'git commit -m "p3: [xv6/pzip]: [whatever update you made]"'
  * type 'git push -u origin master'


## RUN STUFF 
### Parallel Zip
* type 'make' to compile
* type './pzip [args]' to run
* type 'make clean' to start fresh
* Testing
  * type 'make test' to run tests (compare output of my-zip to pzip)
	* any differences will be reported
### xv6 Virtual Memory (debug)
* type 'make qemu-gdb'
* right click on terminal; open second terminal
* on second terminal, type 'gdb'
* in gdb, set breakpoints
  * break main.c:main
  * break [other brk points]
* type 'continue' to let kernel to its thing
* type 'Ctrl-C' to stop the kernel, and go into debugger
* type 'layout source' in gdb to view source code at the same time

## TODO
### Parallel Zip
* understand how threads work...
### xv6 Virtual Memory
* Start read-only

## PROGRESS
### Parallel Zip
### xv6 Virtual Memory
* commented some code so we can understand a little...
* NULL pointer dereference done
  * change makefile in user (makefile.mk) to begin program execution @ address 0x1000 (page 1) instead of 0x000
  * in syscall.c, we need to perform checks for NULL pointers (addr 0)
  * someone told me we need to edit exec.c and vm.c, idk what though
