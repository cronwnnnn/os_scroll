#include "syscall/syscall.h"

extern int32_t trigger_syscall_fork();
extern void trigger_syscall_print(char* str);

int32_t syscall_fork(){
    return trigger_syscall_fork();
}

void syscall_print(char* str) {
  return trigger_syscall_print(str);
}
