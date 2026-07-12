#include <stdint.h>

struct regs {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

static void print_str_at(const char *s, int row) {
    char *vga = (char*)0xB8000 + row * 160;
    for (int i = 0; s[i]; i++) {
        vga[i*2] = s[i];
        vga[i*2+1] = 0x4F; // white on red — makes exceptions obvious
    }
}

static char hex_digit(int v) {
    return v < 10 ? '0' + v : 'A' + (v - 10);
}

void isr_handler(struct regs *r) {
    char buf[32] = "Exception: 0x  ";
    buf[13] = hex_digit((r->int_no >> 4) & 0xF);
    buf[14] = hex_digit(r->int_no & 0xF);
    print_str_at(buf, 1);
    for (;;) { __asm__ volatile ("cli; hlt"); }
}
