#include "gdt.h"
#include "idt.h"

void kmain(void) {
    char *vga = (char*)0xB8000;
    const char *msg = "Kernel booted successfully.";
    for (int i = 0; msg[i]; i++) {
        vga[i*2] = msg[i];
        vga[i*2+1] = 0x0F;
    }
    gdt_init();
    idt_init();

    while (1) {}
}
