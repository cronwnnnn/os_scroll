#ifndef SYSCALL_SYSCALL_H
#define SSYSCALL_SYSCALL_H
#include "common/types.h"

int32_t syscall_fork();
void syscall_print(char* str);
void syscall_exit(int32_t num);

#endif