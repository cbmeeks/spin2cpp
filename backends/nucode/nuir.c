#include "common.h"
#include "nuir.h"
#include <stdio.h>

static const char *NuOpName[] = {
    #define X(m) #m,
    NU_OP_XMACRO
    #undef X
};

static struct NuOpUsage {
    int used;
    int ircode;
    int bytecode;
} opusage[NU_OP_DUMMY];

static int usage_sortfunc(const void *Av, const void *Bv) {
    const struct NuOpUsage *A = Av;
    const struct NuOpUsage *B = Bv;
    /* sort with larger used coming first */
    return B->used - A->used;
}

void NuIrInit() {
    int i;
    for (i = 0; i < NU_OP_DUMMY; i++) {
        opusage[i].used = 0;
        opusage[i].ircode = i;
        opusage[i].bytecode = -1;
    }
}

NuIrLabel *NuCreateLabel() {
    NuIrLabel *r;
    r = calloc(sizeof(*r), 1);
    return r;
}

static NuIr *NuCreateIr() {
    NuIr *r;
    r = calloc(sizeof(*r), 1);
    return r;
}

NuIr *NuEmitOp(NuIrList *irl, NuIrOpcode op) {
    NuIr *r;
    NuIr *last;
    
    r = NuCreateIr();
    r->op = op;
    last = irl->tail;
    irl->tail = r;
    r->prev = last;
    if (last) {
        last->next = r;
    }
    if (!irl->head) {
        irl->head = r;
    }

    if (op < NU_OP_DUMMY) {
        opusage[op].used++;
    }
    return r;
}

NuIr *NuEmitConst(NuIrList *irl, int32_t val) {
    NuIr *r;

    if (val >= -128 && val <= 127) {
        r = NuEmitOp(irl, NU_OP_PUSHI8);
    } else if (val >= -32768 && val <= 32767) {
        r = NuEmitOp(irl, NU_OP_PUSHI16);
    } else {
        r = NuEmitOp(irl, NU_OP_PUSHI32);
    }
    r->val = val;
    return r;
}

NuIr *NuEmitAddress(NuIrList *irl, NuIrLabel *label) {
    NuIr *r = NuEmitOp(irl, NU_OP_PUSHA);
    r->label = label;
    return r;
}

NuIr *NuEmitLabel(NuIrList *irl, NuIrLabel *label) {
    NuIr *r = NuEmitOp(irl, NU_OP_LABEL);
    r->label = label;
    return r;
}

NuIr *NuEmitNamedOpcode(NuIrList *irl, const char *name) {
    NuIrOpcode op;
    for (op = NU_OP_ILLEGAL; op < NU_OP_DUMMY; op++) {
        if (!strcasecmp(NuOpName[op], name)) {
            break;
        }
    }
    if (op == NU_OP_DUMMY) {
        ERROR(NULL, "Unknown opcode %s", name);
        return NULL;
    }
    return NuEmitOp(irl, op);
}


void NuAssignOpcodes()
{
    size_t elemsize = sizeof(opusage[0]);
    unsigned lastelem = (unsigned)NU_OP_DUMMY-1;
    qsort(&opusage, sizeof(opusage) / elemsize, elemsize, usage_sortfunc);
    printf("Most used opcode: %s\n", NuOpName[opusage[0].ircode]);
    printf("Least used opcode: %s\n", NuOpName[opusage[lastelem].ircode]);
}
