#include "common/types.h"
#include "user_sys/syscall.h"

int main(int32_t argc, char* argv[]) {
  syscall_listdir(".");
  return 0;
}
