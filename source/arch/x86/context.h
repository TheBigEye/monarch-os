#ifndef MONARCH_ARCH_X86_CONTEXT_H
#define MONARCH_ARCH_X86_CONTEXT_H 1

#include "base/api/monarch.h"

struct context {
    uintptr_t sp;
};

void context(struct context *ctx, uintptr_t stack, uintptr_t entry);
void contextswitch(uintptr_t *old_sp, uintptr_t new_sp);
int usercall(uintptr_t stack, uintptr_t entry, int argc, char **argv);
void userenter(uintptr_t stack, uintptr_t entry) __attribute__((noreturn));

#endif /* MONARCH_ARCH_X86_CONTEXT_H */
