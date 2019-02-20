CC = gcc
PFLAGS = -c -Wall -Werror -g
CFLAGS = -Wall -Werror -g -pthread -O
SRC=./
OBJ=./obj
ARGS=$(SRC)/r $(SRC)/r $(SRC)/r
TESTS=$(SRC)/sort_* $(SRC)/wordcount_*

time: $(SRC)/mapreduce
	time $(SRC)/mapreduce $(ARGS)

mem: $(SRC)/mapreduce
	valgrind --leak-check=yes $(SRC)/mapreduce $(ARGS)

test: $(SRC)/mapreduce
	$(SRC)/mapreduce

all: $(SRC)/mapreduce

$(OBJ)/mapreduce.o: $(SRC)/mapreduce.c
	$(CC) $(PFLAGS) $^ -o $@

$(SRC)/mapreduce: $(OBJ)/mapreduce.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm ./mapreduce;\
	rm ./*.out;\
	rm $(TESTS);\
	rm ./obj/*;
