#include <stdarg.h>

static int itoa(int val, char *buf) {
    char tmp[12];
    int i = 0, len = 0;
    int neg = val < 0;
    unsigned int uval = neg ? -val : val;

    if (uval == 0) tmp[i++] = '0';
    while (uval) {
        tmp[i++] = '0' + (uval % 10);
        uval /= 10;
    }
    if (neg) buf[len++] = '-';
    while (i > 0) buf[len++] = tmp[--i];
    buf[len] = '\0';
    return len;
}

void ksprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int pos = 0;

    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] == '%' && fmt[i+1] == 'd') {
            int val = va_arg(args, int);
            pos += itoa(val, buf + pos);
            i++;
        } else if (fmt[i] == '%' && fmt[i+1] == 's') {
            const char *s = va_arg(args, const char*);
            while (*s) buf[pos++] = *s++;
            i++;
        } else {
            buf[pos++] = fmt[i];
        }
    }
    buf[pos] = '\0';
    va_end(args);
}
