#include <stdint.h>
#include <stddef.h>

extern uint32_t end; // defined by linker.ld
static uint32_t next_free = 0;

void* kalloc(size_t size) {
    if (next_free == 0) next_free = (uint32_t)&end;
    void *addr = (void*)next_free;
    next_free += size;
    return addr;
}
