#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *fp = NULL;
    char *buffer = NULL;
    size_t len = 0;
    if (argc == 1) {
        printf("my-grep: searchterm [file ...]\n");
        exit(1);
    }
    else if (argc == 2) {
        if ((fp = fopen(argv[1], "r"))) {
            fclose(fp);
        }
        else {
            while (getline(&buffer, &len, stdin) != -1) {
                if (strstr(buffer, argv[1])) {
                    printf("%s", buffer);
                }
            }
            free(buffer);
        }
    }
    else {
        for (int i = 2; i < argc; ++i) {
            if (!(fp = fopen(argv[i], "r"))) {
                printf("my-grep: cannot open file\n");
                free(buffer);
                exit(1);
            }
            while (getline(&buffer, &len, fp) != -1) {
                if (strstr(buffer, argv[1])) {
                    printf("%s", buffer);
                }
            }
            fclose(fp);
        }
        free(buffer);
    }
    return 0;
}
