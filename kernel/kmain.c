#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "serial.h"

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

    while (1) {}
}
