#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERROR "An error has occured\n"
#define PROMPT "wish> "
#define INIT 4
#define EXIT "exit"
#define CD "cd"
#define PATH "path"
#define INIT_PATH "/bin"

struct pair {
  char** list;
  char* link;
  size_t max;
  size_t len;
};

char** 
resize(char*** vec, size_t* pos, size_t* size) { 
  if(*pos == *size) { 
    *size *= 2; 
    *vec = (char**) realloc(*vec, sizeof(char*) * *size); 
  } 
  return *vec; 
}

struct pair*
tokenize(char* cmd) {
  struct pair* p = (struct pair*) malloc(sizeof(struct pair));
  p->list = (char**) malloc(sizeof(char*) * INIT);
  p->link = NULL;
  p->len = 0;
  p->max = INIT;
  size_t pos = 0, base = 0; 
  size_t len = strlen(cmd);
  char state = 0, rflag = 0, c = 0; 
  while(pos < len) {
    c = cmd[pos];
    if(c == ' ' || c == '\t') {
      if(state == 1) {
        if(rflag) {
          p->link = strndup(cmd + base, (pos - base));          
          rflag = 2;
        }
        else {
          resize(&p->list, &p->len, &p->max);
          p->list[p->len++] = strndup(cmd + base, (pos - base));
        }
      }
      state = 0;
    }
    else if(c == '>') {
      if(rflag != 0) {
        for(size_t i = 0; i < p->len; ++i) { free(p->list[i]); }
        free(p->list); free(p->link); free(p); p = NULL;
        state = 10;
        break;
      }
      else if(state == 1) {
        resize(&p->list, &p->len, &p->max);
        p->list[p->len++] = strndup(cmd + base, (pos - base));
      }
      state = 0;
      rflag = 1;
    }
    else {
      if(rflag == 2) {
        for(size_t i = 0; i < p->len; ++i) { free(p->list[i]); }
        free(p->list); free(p->link); free(p); p = NULL;
        state = 10;
        break;
      }
      if(state == 0)
        base = pos;
      state = 1;      
    }
    ++pos;
  }
  if(state == 0 && rflag == 1) {
    for(size_t i = 0; i < p->len; ++i) { free(p->list[i]); }
    free(p->list); free(p->link); free(p); p = NULL;
  }
  else if(state == 1) {
    if(rflag)
      p->link = strndup(cmd + base, (pos - base));
    else {
      resize(&p->list, &p->len, &p->max);
      p->list[p->len++] = strndup(cmd + base, (pos - base));
    }
  }
  return p;
}

int main(int argc, char** argv) {
  if(argc != 1 && argc != 2) {
    fprintf(stderr, ERROR);
    return 1;
  }
  FILE* input = argc == 1 ? stdin : fopen(argv[1], "r");
  if(argc == 2 && !input) {
    fprintf(stderr, ERROR);
    return 1;
  }
  char* buffer; 
  size_t pMax = INIT;
  size_t pCount = 1;
  char** paths = (char**) malloc(sizeof(char*) * INIT);
  if(!paths)  return 1;
  paths[0] = strdup(INIT_PATH);
  size_t len = 0;
  ssize_t lineLen = 0;
  char stop = 0;
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);
  while(!stop) {
    if(argc == 1)
      printf(PROMPT);
    lineLen = getline(&buffer, &len, input);  
    if(lineLen == -1) {
      stop = 1;
      continue;
    }
    buffer[lineLen - 1] = '\0';

    size_t pos = 0, base = 0, cmdPos = 0, cmdSize = INIT; 
    char** cmds = (char**) malloc(sizeof(char*) * INIT); 
    while(pos < lineLen) { 
      if(buffer[pos] == '&') { 
        resize(&cmds, &cmdPos, &cmdSize); 
        cmds[cmdPos++] = strndup(buffer + base, (pos - base)); 
        base = pos + 1; 
      }
      ++pos; 
    } 
    resize(&cmds, &cmdPos, &cmdSize); 
    cmds[cmdPos++] = strndup(buffer + base, len); 

    for(size_t a = 0; a < cmdPos; ++a) {
      struct pair* p = tokenize(cmds[a]);
      if(!p)
        fprintf(stderr, ERROR);
      else {
        if(p->len > 0) {
          if(strcmp(p->list[0], EXIT) == 0) {
            if(p->len > 1)
              fprintf(stderr, ERROR);
            else
              stop = 1;
          }
          else if(strcmp(p->list[0], PATH) == 0) {
            for(size_t x = 0; x < pCount; ++x) { free(paths[x]); }
            pCount = p->len - 1;
            for(size_t x = 0; x < pCount; ++x) {
              size_t l = x;
              resize(&paths, &l, &pMax); 
              paths[x] = strdup(p->list[x + 1]);
            }
          }
          else if(strcmp(p->list[0], CD) == 0) {
            if(p->len != 2 || chdir(p->list[1]))
              fprintf(stderr, ERROR);
          }
          else if(pCount == 0)
            fprintf(stderr, ERROR);
          else {
            char* fPaths[pCount];
            char* format;
            for(size_t x = 0; x < pCount; ++x) {
              format = paths[x][strlen(paths[x]) - 1] == '/' ? "%s%s" : "%s/%s";
              asprintf(&fPaths[x], format, paths[x], p->list[0]); // malloc: check error
            }
            resize(&paths, &pCount, &pMax); 
            paths[pCount] = NULL;
          }
        }
        for(size_t i = 0; i < p->len; ++i) { free(p->list[i]); }
        free(p->list);
        free(p->link);
        free(p);
      }
    }
    for(size_t i = 0; i < cmdPos; ++i) { free(cmds[i]); } 
    free(cmds); 
  }
  free(buffer);
  for(size_t i = 0; i < pCount; ++i) { free(paths[i]); }
  free(paths);
  return 0; 
}
