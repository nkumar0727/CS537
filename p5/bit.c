#include <stdio.h>
#include <stdlib.h>

#define INIT_SIZE 2

uint* refBits = NULL;
uint numRefs = 0;
uint size = INIT_SIZE;

void
initRefs() {
  refBits = (uint*) malloc(sizeof(uint) * INIT_SIZE);
  numRefs = 0;
  size = INIT_SIZE;
}

void
freeRefs() {
  free(refBits);
}

void
printRefs() {
  for(uint i = 0; i < numRefs; ++i) {
    printf("Ref[%u] = %u\n", i, refBits[i]);
  }
}

void
insertRefs(uint ref) {
  if(numRefs >= size) {
    refBits = realloc(refBits, size*2*sizeof(uint));
    size *= 2;
  }
  refBits[numRefs] = ref;
  numRefs += 1;
}


// fills refBits with bits that are set
// numRefs is the size of the list
void
getRefBits_byte(char block) {
  uint mask = 1;
  for(uint bit = 0; bit < 8; ++bit) {
    if(block & mask) {
      printf("Bit %u set\n", bit);
    }
    mask <<= 1;
  }
}

void
getRefBits_block(char* block) {
  for(uint i = 0; i < 512; ++i) {
    uint mask = 1;
    for(uint bit = 0; bit < 8; ++bit) {
      if(block[i] & mask) {
        insertRefs((i*8) + bit);
        //printf("Bit %u set\n", (i*8)+bit);
      }
      mask <<= 1;
    }
  }
}

int
main() {
  //char block = 28;
  char* blk = (char*) malloc(sizeof(char) * 512);
  blk[0] = 28;
  blk[1] = 24;
  for(uint i = 2; i < 512; ++i) {
    blk[i] = 0;
  }
  initRefs();
  getRefBits_block(blk);
  printRefs();
  freeRefs();
  free(blk);
  //getRefBits_byte(block);
//  initRefs();
  //printRefs();
  //freeRefs();
  return 0;
}
