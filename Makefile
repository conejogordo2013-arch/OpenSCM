CC ?= cc
CXX ?= c++
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

ifeq ($(OS),Windows_NT)
EXEEXT ?= .exe
THREAD_LIBS ?=
DL_LIBS ?=
MKDIR_P ?= mkdir
RM_F ?= del /Q
RM_RF ?= rmdir /S /Q
PATH_FIX = $(subst /,\,$1)
else
EXEEXT ?=
THREAD_LIBS ?= -pthread
DL_LIBS ?= -ldl
MKDIR_P ?= mkdir -p
RM_F ?= rm -f
RM_RF ?= rm -rf
PATH_FIX = $1
endif

FFI_CFLAGS ?= $(shell pkg-config --cflags libffi 2>/dev/null)
FFI_LDLIBS ?= $(shell pkg-config --libs libffi 2>/dev/null)
ifneq ($(strip $(FFI_LDLIBS)),)
CFLAGS += -DSCML_USE_LIBFFI=1 $(FFI_CFLAGS)
LDLIBS ?= -lm $(THREAD_LIBS) $(DL_LIBS) $(FFI_LDLIBS)
else
LDLIBS ?= -lm $(THREAD_LIBS) $(DL_LIBS)
endif
CORE_SRC = vm/vm.c compiler/compiler.c compiler/project.c parser/parser.c lexer/lexer.c opcode/opcode.c runtime/scml_runtime_modules.c ffi/scml_ffi.c
DEBUGGER_SRC = debugger/debugger.c
CLI_SRC = main.c $(CORE_SRC)
CLI_OBJ = $(CLI_SRC:.c=.o)
CORE_OBJ = $(CORE_SRC:.c=.o)
DEBUGGER_OBJ = $(DEBUGGER_SRC:.c=.o)
SCML_BIN = bin/scml$(EXEEXT)
GAMEPLAY_BIN = bin/scml_gameplay_example$(EXEEXT)
EDITOR_BIN = bin/scml_editor$(EXEEXT)

all: $(SCML_BIN)

ifneq ($(EXEEXT),)
bin/scml: $(SCML_BIN)
endif

$(SCML_BIN): $(CLI_OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJ) $(LDLIBS)

ifneq ($(EXEEXT),)
bin/scml_gameplay_example: $(GAMEPLAY_BIN)
endif

$(GAMEPLAY_BIN): examples/gameplay_embed.cpp $(CORE_OBJ) | bin
	$(CXX) $(CXXFLAGS) -I. -o $@ examples/gameplay_embed.cpp $(CORE_OBJ) $(LDLIBS)

ifneq ($(EXEEXT),)
bin/scml_editor: $(EDITOR_BIN)
endif

$(EDITOR_BIN): editor/scml_editor.cpp $(CORE_OBJ) $(DEBUGGER_OBJ) | bin
	$(CXX) $(CXXFLAGS) -I. -o $@ editor/scml_editor.cpp $(CORE_OBJ) $(DEBUGGER_OBJ) $(LDLIBS)

bin:
	$(MKDIR_P) $(call PATH_FIX,bin)

examples/gameplay.scmlbin: $(SCML_BIN) examples/gameplay.scml stscm/stscm.scml
	$(SCML_BIN) compile examples/gameplay.scml $@

cpp-example: $(GAMEPLAY_BIN) examples/gameplay.scmlbin
	$(GAMEPLAY_BIN)

editor-example: $(EDITOR_BIN) examples/gameplay.scml
	$(EDITOR_BIN) examples/gameplay.scml examples/editor_tmp.scmlbin

clean:
	-$(RM_F) $(call PATH_FIX,$(CLI_OBJ) $(DEBUGGER_OBJ) bin/scml bin/scml.exe bin/scml_gameplay_example bin/scml_gameplay_example.exe bin/scml_editor bin/scml_editor.exe examples/*.scmlbin log.txt)

.PHONY: all clean cpp-example editor-example core test doctor tooling migration-audit verify-examples

core: $(SCML_BIN)

tooling:
	@echo "Tooling scripts:"
	@echo "  - tools/scml_doctor.sh"
	@echo "  - tests/smoke_suite.sh"

test: $(SCML_BIN)
	bash tests/smoke_suite.sh

doctor: $(SCML_BIN)
	bash tools/scml_doctor.sh

migration-audit:
	bash tools/scml_migration_audit.sh

verify-examples:
	bash tools/scml_verify_examples.sh
