#include "io.h"

#define PIC1        0x20
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1+1)
#define PIC2        0xA0
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2+1)

#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01

void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    outb(PIC1_DATA, 0x20); // master PIC vector offset -> 32
    outb(PIC2_DATA, 0x28); // slave PIC vector offset -> 40

    outb(PIC1_DATA, 4);    // tell master about slave at IRQ2
    outb(PIC2_DATA, 2);    // tell slave its cascade identity

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}
