#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_WRITE  1
#define SYS_GETPID 2
#define SYS_ALLOC  3
#define SYS_EXIT   4

void syscall_init(void);
int syscall_invoke(int num, int arg1, int pid);

#endif
