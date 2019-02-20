CC = gcc 
PFLAGS = -c -Wall -Werror -g
CFLAGS = -Wall -Werror -g
SRC=./
OBJ=./obj

otest: all
	~cs537-1/tests/bin/test-wish.csh -v

atest: all
	~cs537-1/tests/bin/test-wish.csh -c -v

val: $(SRC)/wish
	valgrind --leak-check=yes ./wish

sec: $(SRC)/wish2

$(SRC)/wish2: $(OBJ)/wish2.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ)/wish2.o: $(SRC)/wish2.c
	$(CC) $(PFLAGS) $^ -o $@

all: $(SRC)/wish

$(SRC)/wish: $(OBJ)/wish.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ)/wish.o: $(SRC)/wish.c
	$(CC) $(PFLAGS) $^ -o $@


sh: $(SRC)/shell

$(SRC)/shell: $(OBJ)/shell.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ)/shell.o: $(SRC)/shell.c
	$(CC) $(PFLAGS) $^ -o $@

clean:
	rm -rf ./wish;\
	rm -rf ./wish2;\
	rm -rf ./obj/*;\
	rm -rf ./shell
