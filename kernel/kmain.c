#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "serial.h"
#include "syscall.h"
#include "mem.h"

void kmain(void) {
    char *vga = (char*)0xB8000;
    const char *msg = "Kernel booted successfully.";
    for (int i = 0; msg[i]; i++) {
        vga[i*2] = msg[i];
        vga[i*2+1] = 0x0F;
    }

    gdt_init();
    idt_init();
    pic_remap();
    serial_init();
    serial_write("Kernel boot: GDT, IDT, PIC, serial all initialized.\n");

    syscall_init();

    // Simulated normal process (pid 1)
    syscall_invoke(SYS_WRITE, 0, 1);
    syscall_invoke(SYS_ALLOC, 64, 1);
    syscall_invoke(SYS_WRITE, 0, 1);
    syscall_invoke(SYS_EXIT, 0, 1);

    // Simulated normal process (pid 2)
    syscall_invoke(SYS_GETPID, 0, 2);
    syscall_invoke(SYS_WRITE, 0, 2);
    syscall_invoke(SYS_EXIT, 0, 2);

    // Simulated anomalous process (pid 3) - tight ALLOC loop, no WRITE/EXIT pattern
    for (int i = 0; i < 10; i++) {
        syscall_invoke(SYS_ALLOC, 4, 3);
    }

    while (1) {}
}
