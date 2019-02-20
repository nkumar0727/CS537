 #define DEBUG 0 
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#ifdef DEBUG
#include <errno.h> // debug
#endif

#define PROMPT "wish> "
#define EXIT "exit"
#define CD "cd"
#define PATH "path"
#define SPACE "\t "
#define ERROR "An error has occurred\n" // IMPORTANT: use for all errors 
#define INIT_PATH "/bin" 
#define SL '/'
#define INIT_LEN 4
#define REDIR ">" 
#define CREDIR '>'
#define PAR "&"

int
main(int argc, char** argv) {

  // INIT STUFF 
  int errCode = 0;
  size_t pMax = INIT_LEN;
  size_t pCount = 1; 
  char** paths = (char**) malloc(sizeof(char*) * INIT_LEN); // expand if needed 
  if(!paths)
    exit(1);
  paths[0] = strdup(INIT_PATH); // uses malloc; check error
  char* buffer = NULL;
  size_t len = 0;
  ssize_t lineLen = 0;
  char stop = 0;

  if(argc == 1 || argc == 2) { 

    // SETUP BATCH vs INTERACTIVE 
    setbuf(stdout, NULL); // need this for stupid stdout buffering problems
    setbuf(stderr, NULL);
    FILE* input = argc == 1 ? stdin : fopen(argv[1], "r");
    if(argc == 2 && !input) {
      fprintf(stderr, ERROR);
      errCode = 1;
        /** printf("I am setting errCode to 1 @ 52\n"); */
      stop = 1;
    }

    // MAIN SHELL
    /** size_t count = 0;
      * stop = stop; */
    while(!stop) {

      // PROMPT AND SETUP
      if(argc == 1)
       printf(PROMPT);
      lineLen = getline(&buffer, &len, input);
      if(lineLen == -1) { // hit EOF marker
        stop = 1;
        /** printf("I am setting errCode to 1 @ 67\n"); */
        continue;
      }
      buffer[lineLen-1] = '\0';

      // TOKENIZE

      // stores each commmand to be excuted in parallel
      char** parCmds = (char**) malloc(sizeof(char*) * INIT_LEN);
      if(!parCmds) { // malloc fails
        fprintf(stderr, ERROR);
        continue;
      }
      size_t cmdLoc = 0;
      size_t parCmdSz = INIT_LEN;
      char* cmd = strtok(buffer, PAR);
      if(cmd)
        parCmds[cmdLoc++] = strdup(cmd); // uses malloc; check error
      while((cmd = strtok(NULL, PAR))) {
        if(cmdLoc == parCmdSz) {
          parCmdSz *= 2;
          parCmds = (char**) realloc(parCmds, sizeof(char*) * parCmdSz); // check error
        }
        parCmds[cmdLoc++] = strdup(cmd); // uses malloc; check error
      }

      // TODO: if one parallel cmd poses error, do others execute?
      // for each parallel command, tokenize by redir >
      for(size_t i = 0; i < cmdLoc; ++i) {
        char rflag = 0;
        // preemptive scan for multiple redirs >
        char* lrloc = strchr(parCmds[i], CREDIR);
        char* rrloc = strrchr(parCmds[i], CREDIR);
        if(lrloc != rrloc) {
          fprintf(stderr, ERROR); // TODO: if one parallel cmd fails, what of the rest?
          // assume that if one fails, it all fails...
          break;
        }
        rflag = lrloc ? 1 : 0; // lrloc == rrloc, so if > is detected, set flag
        size_t tokLoc = 0;
        size_t tokSz = INIT_LEN;
        char** cmdTok = (char**) malloc(sizeof(char*) * INIT_LEN);
        if(!cmdTok) { // malloc fails
          fprintf(stderr, ERROR);
          break;
        }
        char* tok = strtok(parCmds[i], REDIR);
        if(tok)
          cmdTok[tokLoc++] = strdup(tok); // uses malloc; check error
        while((tok = strtok(NULL, REDIR))) {
          if(tokLoc == tokSz) {
            tokSz *= 2;
            cmdTok = (char**) realloc(cmdTok, sizeof(char*) * tokSz); // check error
          }
          cmdTok[tokLoc++] = strdup(tok); // uses malloc; check error
        }
        if(tokLoc == 1 || tokLoc == 2) {

          // now tokenize based on whitespace
          size_t subTokLoc = 0;
          size_t subTokSz = INIT_LEN;
          char** normTok = (char**) malloc(sizeof(char*) * INIT_LEN);
          if(!normTok) { // malloc fails
            fprintf(stderr, ERROR);
            for(size_t b = 0; b < subTokLoc; ++b) { free(normTok[b]); }
            free(normTok);
            break;
          }
          // stuff before redir
          char* subTok = strtok(cmdTok[0], SPACE);
          if(subTok)
            normTok[subTokLoc++] = strdup(subTok); // check error
          while((subTok = strtok(NULL, SPACE))) {
            if(subTokLoc == subTokSz) {
              subTokSz *= 2;
              normTok = (char**) realloc(normTok, sizeof(char*) * subTokSz); // check error
            }
            normTok[subTokLoc++] = strdup(subTok); // uses malloc; check error
          }

          // fout for redir
          if(tokLoc == 2) 
            subTok = strtok(cmdTok[1], SPACE);
          if((tokLoc == 2 && (!subTok || strtok(NULL, SPACE))) || (tokLoc == 1 && rflag)) // excess or lacking fout for redir
            fprintf(stderr, ERROR);
          else if(subTokLoc > 0){ // good command
            // process command normTok[x] (cmd / args), subTok (fout)
            if(strcmp(normTok[0], EXIT) == 0) { // BUILTIN-EXIT                        
              if(subTokLoc > 1)
                fprintf(stderr, ERROR);
              else
                stop = 1;
            }
            else if(strcmp(normTok[0], CD) == 0) { // BUILTIN-CD
              if(subTokLoc != 2 || chdir(normTok[1]))
                fprintf(stderr, ERROR);
            }
            else if(strcmp(normTok[0], PATH) == 0) { // BUILTIN-PATH
              #ifdef DEBUG
              printf("Old Paths: ");
              for(size_t p = 0; p < pCount; ++p) {
                printf("%s ", paths[p]);
              }
              printf("\n");
              #endif

              for(size_t p = 0; p < pCount; ++p) { free(paths[p]); }
              pCount = subTokLoc - 1;
              for(size_t p = 0; p < pCount; ++p) {
                if(p == pMax) { // expand path list
                  pMax *= 2;
                  paths = (char**) realloc(paths, sizeof(char*) * pMax); // check error
                }
                paths[p] = strdup(normTok[p + 1]);
              }
              #ifdef DEBUG
              printf("New Paths: ");
              for(size_t p = 0; p < pCount; ++p) {
                printf("%s ", paths[p]);
              }
              printf("\n");
              #endif
            }
            else if(pCount == 0) // no path for running programs
              fprintf(stderr, ERROR);
            else { // RUN PROGRAMS
          
              #ifdef DEBUG
              for(size_t b = 0; b < subTokLoc; ++b) {
                printf("Token: %lu [%s]\n", b, normTok[b]);
              }
              if(tokLoc == 2 && subTok)
                printf("Fout: %s\n", subTok);
              #endif

              // setup all possible paths for program
              char* fPaths[pCount];
              char* format;
              for(size_t p = 0; p < pCount; ++p) {
                format = paths[p][strlen(paths[p]) - 1] == SL ? "%s%s" : "%s/%s";
                asprintf(&fPaths[p], format, paths[p], normTok[0]); // malloc: check error
              }

              // dont forget extra NULL @ end of command
              if(subTokLoc == subTokSz) // check error
                normTok = (char**) realloc(normTok, sizeof(char*) * (subTokSz + 1)); 
              normTok[subTokLoc] = NULL;

              char eflag = 1;
              for(size_t p = 0; p < pCount; ++p) { // look through all paths for program
                if(!access(fPaths[p], X_OK)) {
                  eflag = 0;
                  pid_t rc = fork();
                  if(rc < 0) // report an error when fork fails? 
                    fprintf(stderr, ERROR); 
                  else if(rc == 0) { // child proc
                    if(rflag) { // redirect output
                      freopen(subTok, "w", stdout); // these can fail; check error
                      freopen(subTok, "w", stderr);
                    }
                    int ec = execv(fPaths[p], normTok);
                    if(ec == -1) // could not execute error
                      fprintf(stderr, ERROR);
                  }
                  else { // parent proc
                    /** free(subTok); */
                    int wc = wait(NULL);                    
                    if(wc == -1) // parent could not wait properly...
                      fprintf(stderr, ERROR);
                  }
                  break; // only use 1st successful path 
                }
              }
              if(eflag) // could not find any executables in paths
                fprintf(stderr, ERROR);
              for(size_t p = 0; p < pCount; ++p) { free(fPaths[p]); }
            }
          }

          for(size_t b = 0; b < subTokLoc; ++b) { free(normTok[b]); }
          free(normTok);
        }
        else
          fprintf(stderr, ERROR);

        for(size_t a = 0; a < tokLoc; ++a) { free(cmdTok[a]); }
        free(cmdTok);
      }
// end of parallel loop
      for(size_t i = 0; i < cmdLoc; ++i) { free(parCmds[i]); }
      free(parCmds);
    }

    // cleanup
    free(buffer);
    for(size_t i = 0; i < pCount; ++i) { free(paths[i]); }
    free(paths);
  }
  else {
    fprintf(stderr, ERROR);
    errCode = 1;
  }
  /** printf("ErrorCode: %d\n", errCode); */
  exit(errCode);
}
