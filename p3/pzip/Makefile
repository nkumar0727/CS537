CC = gcc 
PFLAGS = -c -Wall -Werror -g
CFLAGS = -Wall -Werror -pthread -O -g # this last part is for gdb debugging
SRC=./
TEST=./test
OBJ=./obj
TESTFILES=$(TEST)/test1.txt $(TEST)/test2.txt $(TEST)/test3.txt


all: $(SRC)/pzip

test: all
	rm -rf $(SRC)/my-zip.out;\
	rm -rf $(SRC)/pzip.out;\
	$(SRC)/my-zip f_tiny.txt > my-zip.out;\
	$(SRC)/pzip f_tiny.txt > pzip.out;\
	diff -y -s $(SRC)/my-zip.out $(SRC)/pzip.out

$(OBJ)/pzip.o: $(SRC)/pzip.c
	$(CC) $(PFLAGS) $^ -o $@ 

$(SRC)/pzip: $(OBJ)/pzip.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf $(TESTFILES);\
	rm -rf $(TEST)/my-zip.out $(TEST)/pzip.out;\
	rm -rf ./pzip;\
	rm -rf ./my-zip;\
	rm -rf ./obj/*;
