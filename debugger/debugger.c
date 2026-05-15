#include "debugger.h"

#include <stdio.h>
#include <string.h>

void scml_debugger_init(ScmlDebugger *debugger, ScmlVM *vm) {
    memset(debugger, 0, sizeof(*debugger));
    debugger->vm = vm;
}

int scml_debugger_add_pc_breakpoint(ScmlDebugger *debugger, uint32_t pc) {
    if (!debugger || debugger->breakpoint_count >= SCML_DEBUGGER_MAX_BREAKPOINTS) return 0;
    debugger->breakpoints[debugger->breakpoint_count++] = pc;
    return 1;
}

int scml_debugger_add_label_breakpoint(ScmlDebugger *debugger, const char *label) {
    uint32_t pc = 0;
    if (!debugger || !scml_vm_find_label(debugger->vm, label, &pc)) return 0;
    return scml_debugger_add_pc_breakpoint(debugger, pc);
}

int scml_debugger_is_breakpoint(const ScmlDebugger *debugger, uint32_t pc) {
    if (!debugger) return 0;
    for (size_t i = 0; i < debugger->breakpoint_count; i++) {
        if (debugger->breakpoints[i] == pc) return 1;
    }
    return 0;
}

int scml_debugger_step(ScmlDebugger *debugger, char *err, size_t err_size) {
    if (!debugger || !debugger->vm) return 0;
    debugger->paused = 0;
    int rc = scml_vm_step(debugger->vm, err, err_size);
    if (rc > 0 && scml_debugger_is_breakpoint(debugger, (uint32_t)scml_vm_pc(debugger->vm))) debugger->paused = 1;
    return rc;
}

int scml_debugger_continue(ScmlDebugger *debugger, char *err, size_t err_size) {
    if (!debugger || !debugger->vm) return 0;
    debugger->paused = 0;
    for (;;) {
        int rc = scml_vm_step(debugger->vm, err, err_size);
        if (rc <= 0) return rc;
        if (scml_debugger_is_breakpoint(debugger, (uint32_t)scml_vm_pc(debugger->vm))) {
            debugger->paused = 1;
            return 1;
        }
    }
}

void scml_debugger_dump_state(ScmlDebugger *debugger, FILE *out) {
    if (!debugger || !debugger->vm || !out) return;
    fprintf(out, "SCML debugger: pc=%zu line=%u paused=%d breakpoints=%zu\n",
            scml_vm_pc(debugger->vm), scml_vm_current_line(debugger->vm), debugger->paused, debugger->breakpoint_count);
    scml_vm_dump_memory(debugger->vm, out);
}
