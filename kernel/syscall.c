#include "syscall.h"
#include "serial.h"
#include "kprintf.h"
#include "mem.h"
#include <stdint.h>
#include <stddef.h>


static uint32_t tick = 0;

static void syscall_log(int pid, int syscall_num, int arg) {
    char buf[64];
    tick++;
    ksprintf(buf, "[%d] pid=%d syscall=%d arg=%d\n", (int)tick, pid, syscall_num, arg);
    serial_write(buf);
}

int syscall_invoke(int num, int arg1, int pid) {
    syscall_log(pid, num, arg1);

    switch (num) {
        case SYS_WRITE:
            serial_write("  -> SYS_WRITE\n");
            return 0;
        case SYS_GETPID:
            return pid;
        case SYS_ALLOC:
            kalloc((size_t)arg1);
            return 0;
        case SYS_EXIT:
            serial_write("  -> SYS_EXIT\n");
            return 0;
        default:
            serial_write("  -> UNKNOWN SYSCALL\n");
            return -1;
    }
}

void syscall_init(void) {
    serial_write("Syscall interface ready.\n");
}
