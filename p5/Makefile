CC = gcc
PFLAGS = -c -Wall -Werror -g
CFLAGS = -Wall -Werror -g -O
SRC=./

all: $(SRC)/xcheck

test: $(SRC)/xcheck
	~cs537-1/ta/tests/5a-new/runtests

$(SRC)/xcheck.o: $(SRC)/xcheck.c
	$(CC) $(PFLAGS) $^ -o $@

$(SRC)/xcheck: $(SRC)/xcheck.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm ./xcheck;\
	rm ./*.o;
