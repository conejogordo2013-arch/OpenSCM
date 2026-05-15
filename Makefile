CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic
SRC = main.c vm/vm.c compiler/compiler.c parser/parser.c lexer/lexer.c opcode/opcode.c
OBJ = $(SRC:.c=.o)

all: bin/scml

bin/scml: $(OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $(OBJ)

bin:
	mkdir -p bin

clean:
	rm -f $(OBJ) bin/scml examples/*.scmlbin log.txt

.PHONY: all clean
