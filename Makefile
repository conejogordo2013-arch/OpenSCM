CC ?= cc
CXX ?= c++
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CORE_SRC = vm/vm.c compiler/compiler.c parser/parser.c lexer/lexer.c opcode/opcode.c
DEBUGGER_SRC = debugger/debugger.c
CLI_SRC = main.c $(CORE_SRC)
CLI_OBJ = $(CLI_SRC:.c=.o)
CORE_OBJ = $(CORE_SRC:.c=.o)
DEBUGGER_OBJ = $(DEBUGGER_SRC:.c=.o)

all: bin/scml

bin/scml: $(CLI_OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJ)

bin/scml_gameplay_example: examples/gameplay_embed.cpp $(CORE_OBJ) | bin
	$(CXX) $(CXXFLAGS) -I. -o $@ examples/gameplay_embed.cpp $(CORE_OBJ)

bin/scml_editor: editor/scml_editor.cpp $(CORE_OBJ) $(DEBUGGER_OBJ) | bin
	$(CXX) $(CXXFLAGS) -I. -o $@ editor/scml_editor.cpp $(CORE_OBJ) $(DEBUGGER_OBJ)

bin:
	mkdir -p bin

examples/gameplay.scmlbin: bin/scml examples/gameplay.scml stscm/stscm.scml
	bin/scml compile examples/gameplay.scml $@

cpp-example: bin/scml_gameplay_example examples/gameplay.scmlbin
	bin/scml_gameplay_example

editor-example: bin/scml_editor examples/gameplay.scml
	bin/scml_editor examples/gameplay.scml examples/editor_tmp.scmlbin

clean:
	rm -f $(CLI_OBJ) $(DEBUGGER_OBJ) bin/scml bin/scml_gameplay_example bin/scml_editor examples/*.scmlbin log.txt

.PHONY: all clean cpp-example editor-example
