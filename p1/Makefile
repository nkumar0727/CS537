CC = clang-6.0 
PFLAGS = -c -Wall -Werror -g
CFLAGS = -Wall -g -Werror
SRC=./
OBJ=./obj

all: cat grep zip unzip

testcat:
	~cs537-1/tests/bin/test-my-cat.csh -c -v

testgrep:
	~cs537-1/tests/bin/test-my-grep.csh -c -v

testzip:
	~cs537-1/tests/bin/test-my-zip.csh -c -v

testun:
	~cs537-1/tests/bin/test-my-unzip.csh -v

szip:
	$(CC) -Wa,-adhln -g -Wall -Werror $(SRC)/my-zip.c > $(SRC)/my-zip.s

cat: $(SRC)/my-cat

grep: $(SRC)/my-grep

zip: $(SRC)/my-zip

unzip: $(SRC)/my-unzip

$(OBJ)/my-cat.o: $(SRC)/my-cat.c
	$(CC) $(PFLAGS) $^ -o $@ 

$(SRC)/my-cat: $(OBJ)/my-cat.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ)/my-grep.o: $(SRC)/my-grep.c
	$(CC) $(PFLAGS) $^ -o $@ 

$(SRC)/my-grep: $(OBJ)/my-grep.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ)/my-zip.o: $(SRC)/my-zip.c
	$(CC) $(PFLAGS) $^ -o $@ 

$(SRC)/my-zip: $(OBJ)/my-zip.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ)/my-unzip.o: $(SRC)/my-unzip.c
	$(CC) $(PFLAGS) $^ -o $@ 

$(SRC)/my-unzip: $(OBJ)/my-unzip.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf ./my-cat;\
	rm -rf ./my-grep;\
	rm -rf ./my-zip;\
	rm -rf ./my-unzip;\
	rm -rf ./obj/*;
