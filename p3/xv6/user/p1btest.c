#include "types.h" 
#include "user.h"

#define STDOUT 1

int
main(int argc, char* argv[]) {
  int rc1;
  rc1 = getreadcount();
  printf(STDOUT, "read() has been called %d times\n", rc1);
  exit();
}
