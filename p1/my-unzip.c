#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

int
main(int argc, char** argv) {
  struct stat f_st;
  size_t f_sizes[argc];
  size_t total_sz = 0;
  int last_file = argc;

  for(int i = 1; i < argc; ++i) {
    if(stat(argv[1], &f_st) == -1) {
      printf("my-unzip: cannot open file\n");
      last_file = i;
      break; 
    }
    f_sizes[i-1] = f_st.st_size;    
    total_sz += f_st.st_size;
  }
  if(total_sz == 0) {
    printf("my-unzip: file1 [file2 ...]\n");
    exit(1);
  }

  // void*
  char* hndl = (char*) malloc(total_sz);
  size_t offset = 0;
  for(int i = 1; i < last_file; ++i) {
    FILE* fp = fopen(argv[i], "r");
    if(!fread((void*) (hndl + offset), 1, f_sizes[i-1], fp)) {
      printf("Could not read properly\n");
      free(hndl);
      fclose(fp);
      exit(1);
    }
    offset += f_sizes[i-1];
    fclose(fp);
  }

  // perform run-length decoding
  unsigned int pos = 0;
  unsigned int freq = 0;
  char enc = 0;
  while(pos < total_sz) {
    freq = hndl[pos];
    enc = hndl[pos + 4];
    freq = *(unsigned int*)(hndl + pos);
    enc = *(char*)(hndl + pos + 4);
    for(unsigned int i = 0; i < freq; ++i) {
      putc(enc, stdout);
    }
    pos += 5; 
  }

  free(hndl);
  return 0;
}
