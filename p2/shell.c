#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_SZ 4
#define PARDELIM "&"
#define REDIR ">"
#define CREDIR '>'
#define SPACE " \t"

// tokenize input based on parallel commands, then redirection
int
main(int argc, char** argv) {
  char input[1024];
  sprintf(input, "cmd1 > args0>fout0 & cmd2 args2 > fout1 & cmd3 args1> fout2 & cmd4 args4 & cmd5 args5 args6 args7 args8>fout10");
  printf("CMD LENGTH: %lu\n", strlen(input));
  size_t parCmdSz = INIT_SZ;
  char** parCmds = (char**) malloc(sizeof(char*) * INIT_SZ); // each command in parallel
  if(!parCmds) {
    fprintf(stderr, "malloc failed\n");
    return 1;
  }

  // for the whole line, tokenize based on &
  char* cmd = strtok(input, PARDELIM);
  size_t cmdLoc = 0;
  if(cmd)
    parCmds[cmdLoc++] = strdup(cmd);
  while((cmd = strtok(NULL, PARDELIM))) { 
    if(cmdLoc == parCmdSz) {
      parCmdSz *= 2;
      parCmds = (char**) realloc(parCmds, sizeof(char*) * parCmdSz);
    }
    parCmds[cmdLoc++] = strdup(cmd);
  }

  // for each command separated by & tokenize based on >
  for(size_t i = 0; i < cmdLoc; ++i) {
    printf("Command #%lu: [%s]\n", i, parCmds[i]);
    size_t tokLoc = 0;
    size_t tokSz = 0;
    char** cmdTok = (char**) malloc(sizeof(char*) * INIT_SZ);
    if(!cmdTok) { // catastophic failure
      fprintf(stderr, "malloc failed\n");  
      break;
    }
    char* tok = strtok(parCmds[i], REDIR);
    if(tok)
      cmdTok[tokLoc++] = strdup(tok);
    while((tok = strtok(NULL, REDIR))) {
      if(tokLoc == tokSz) {
        tokSz *= 2;
        cmdTok = (char**) realloc(cmdTok, sizeof(char*) * tokSz);
      }
      cmdTok[tokLoc++] = strdup(tok);
    }
    if(tokLoc == 1 || tokLoc == 2) {
      
      // now, tokenize based on whitespace
      size_t subTokLoc = 0;
      size_t subTokSz = 0;
      char** normTok = (char**) malloc(sizeof(char*) * INIT_SZ);
      /** char* fout = NULL; */
      if(!normTok) {
        fprintf(stderr, "malloc failed\n");
        break;
      }

      // stuff before redir
      char* subTok = strtok(cmdTok[0], SPACE);
      if(subTok)
        normTok[subTokLoc++] = strdup(subTok);
      while((subTok = strtok(NULL, SPACE))) {
        if(subTokLoc == subTokSz) {
          subTokSz *= 2;
          normTok = (char**) realloc(normTok, sizeof(char*) * subTokSz);
        }
        normTok[subTokLoc++] = strdup(subTok);
      }

      // fout for redir
      subTok = strtok(cmdTok[1], SPACE);           
      if(!subTok)
        printf("Bad command -- no fout for redir");
      else {
        if(strtok(NULL, SPACE))
          printf("Bad command -- too many fout for redir");
        else { // good cmd
          for(size_t b = 0; b < subTokLoc; ++b) {
            printf("Token: %lu [%s]\n", b, normTok[b]);
          }
          if(tokLoc == 2 && subTok)
            printf("Fout: %s\n", subTok);
        }
      }
// ls, path, path /usr/bin /bin /boot, ls      
      for(size_t b = 0; b < subTokLoc; ++b) { free(normTok[b]); }
      /** free(fout); */
      free(normTok);
    }
    else
      printf("Error: Bad command -- too many redir\n");

    for(size_t a = 0; a < tokLoc; ++a) { free(cmdTok[a]); }
    free(cmdTok);
  }
  for(size_t i = 0; i < cmdLoc; ++i) { free(parCmds[i]); }
  free(parCmds);
  return 0;
}
