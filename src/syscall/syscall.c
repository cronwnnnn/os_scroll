#include "syscall/syscall.h"

extern int32_t trigger_syscall_fork();
extern void trigger_syscall_print(char* str);
extern void trigger_syscall_exit(int32_t num);
extern int32_t trigger_syscall_exec(char* file_path, int32_t argc, char** agrv);
extern int32_t trigger_syscall_read_char();;
extern int32_t trigger_syscall_wait(uint32_t pid, uint32_t* status);

int32_t syscall_fork(){
    return trigger_syscall_fork();
}

void syscall_print(char* str) {
  return trigger_syscall_print(str);
}

void syscall_exit(int32_t num) {
  return trigger_syscall_exit(num);
}

int32_t syscall_exec(char* file_path, int32_t argc, char** argv){
  return trigger_syscall_exec(file_path, argc, argv);
}

int32_t syscall_read_char() {
  return trigger_syscall_read_char();
}

int32_t syscall_wait(uint32_t pid, uint32_t* status) {
  return trigger_syscall_wait(pid, status);
}
