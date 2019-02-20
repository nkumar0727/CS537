#include <stdio.h>
#include <stdlib.h>

#define BUF_SZ 128

int main(int argc, char **argv) {
    if (argc > 1) {
        FILE *fp = NULL;
        char buffer[BUF_SZ];
        for (int i = 1; i < argc; ++i) {
            if (!(fp = fopen(argv[i], "r"))) {
                printf("my-cat: cannot open file\n");
                exit(1);
            }
            while (fgets(buffer, BUF_SZ, fp)) {
                printf("%s", buffer);
            }
            fclose(fp);
        }
    }
    return 0;
}
