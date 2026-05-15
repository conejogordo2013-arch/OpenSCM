#ifndef SCML_DEBUGGER_H
#define SCML_DEBUGGER_H

#include "../vm/vm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCML_DEBUGGER_MAX_BREAKPOINTS 128

typedef struct ScmlDebugger {
    ScmlVM *vm;
    uint32_t breakpoints[SCML_DEBUGGER_MAX_BREAKPOINTS];
    size_t breakpoint_count;
    int paused;
} ScmlDebugger;

void scml_debugger_init(ScmlDebugger *debugger, ScmlVM *vm);
int scml_debugger_add_pc_breakpoint(ScmlDebugger *debugger, uint32_t pc);
int scml_debugger_add_label_breakpoint(ScmlDebugger *debugger, const char *label);
int scml_debugger_is_breakpoint(const ScmlDebugger *debugger, uint32_t pc);
int scml_debugger_step(ScmlDebugger *debugger, char *err, size_t err_size);
int scml_debugger_continue(ScmlDebugger *debugger, char *err, size_t err_size);
void scml_debugger_dump_state(ScmlDebugger *debugger, FILE *out);

#ifdef __cplusplus
}
#endif

#endif
