#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define BUF_SZ 128
#define MAX_FRQ (~0)

int main(int argc, char **argv)
{

  struct stat f_st;
  size_t f_sizes[argc];
  size_t total_sz = 0;
  int last_file = argc;

  for (int i = 1; i < argc; ++i)
  {
    if (stat(argv[1], &f_st) == -1)
    {
      printf("my-zip: cannot open file\n");
      last_file = i;
      break;
    }
    f_sizes[i - 1] = f_st.st_size;
    total_sz += f_st.st_size;
  }
  if (total_sz == 0)
  {
    printf("my-zip: file1 [file2 ...]\n");
    exit(1);
  }

  char *hndl = (char *)malloc(total_sz + 1);
  size_t offset = 0;
  for (int i = 1; i < last_file; ++i)
  {
    FILE *fp = fopen(argv[i], "r");
    if (!fread((void *)(hndl + offset), sizeof(char), f_sizes[i - 1], fp))
    {
      printf("Could not read properly\n");
      free(hndl);
      fclose(fp);
      exit(1);
    }
    offset += f_sizes[i - 1];
    fclose(fp);
  }
  hndl[total_sz] = '\0';

  // perform RLE char by char
  size_t pos = 0;
  unsigned int freq = 1;
  char curr, enc;
  curr = hndl[pos++];
  enc = curr;
  while (pos < total_sz)
  {
    curr = hndl[pos++];
    if (curr != enc)
    {
      fwrite(&freq, sizeof(unsigned int), 1, stdout);
      putc(enc, stdout);
      enc = curr;
      freq = 1;
    }
    else if (freq < MAX_FRQ)
      ++freq;
    else
    {
      fwrite(&freq, sizeof(unsigned int), 1, stdout);
      putc(enc, stdout);
      freq = 0;
    }
  }
  if (total_sz == 1)
  {
    fwrite(&freq, sizeof(unsigned int), 1, stdout);
    putc(enc, stdout);
  }
  else if (freq >= 1)
  {
    fwrite(&freq, sizeof(unsigned int), 1, stdout);
    putc(enc, stdout);
  }

  free(hndl);

  return 0;
}
