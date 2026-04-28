#include "syscall/syscall.h"

extern int32_t trigger_syscall_fork();
extern void trigger_syscall_print(char* str);
extern void trigger_syscall_exit(int32_t num);

int32_t syscall_fork(){
    return trigger_syscall_fork();
}

void syscall_print(char* str) {
  return trigger_syscall_print(str);
}

void syscall_exit(int32_t num) {
  return trigger_syscall_exit(num);
}
