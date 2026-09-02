#include "arch/x86/context.h"

void context(struct context *ctx, uintptr_t stack, uintptr_t entry) {
    uintptr_t *sp = (uintptr_t *)stack;

    *--sp = entry; /* return address after restored registers */
    *--sp = 0;     /* ebp */
    *--sp = 0;     /* ebx */
    *--sp = 0;     /* esi */
    *--sp = 0;     /* edi */

    ctx->sp = (uintptr_t)sp;
}
