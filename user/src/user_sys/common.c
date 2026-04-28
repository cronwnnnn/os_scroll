#include "syscall/syscall.h"
#include "user_sys/common.h"

extern int main(int32_t, char**);

void _start(int32_t argc, char** argv) {
  int exit_code = main(argc, argv);
  syscall_exit(exit_code);
}
