#ifndef SYSCALL_SYSCALL_H
#define SSYSCALL_SYSCALL_H
#include "common/types.h"

int32_t syscall_fork();
void syscall_print(char* str);
void syscall_exit(int32_t num);
int32_t syscall_exec(char* file_path, int32_t argc, char** argv);
int32_t syscall_read_char();
int32_t syscall_wait(uint32_t pid, uint32_t* status);

#endif