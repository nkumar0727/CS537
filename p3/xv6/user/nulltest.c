#include "types.h" 
#include "user.h"

#define STDOUT 1

int
main(int argc, char* argv[]) {
	int* nullptr = NULL;
	int cSize = sizeof(char);
	for(int i = 0; i < 20; ++i) {
		printf(STDOUT, "@ addr: %d is %c\n", cSize * i, nullptr[i]);
	}
	/** printf(STDOUT, "Value stored %d\n", nullptr); */
	exit();
}
