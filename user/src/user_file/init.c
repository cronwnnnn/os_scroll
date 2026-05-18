#include "user_sys/syscall.h"
#include "common/types.h"
#include "user_sys/stdlib.h"
#include "user_sys/user_io.h"

int main(int32_t argc, char** agrv){
    printf("successfully load user file!!!!\n");
    
    int32_t pid = syscall_fork();
    if(pid < 0){
        syscall_print("fork failed\n");
    }else if(pid > 0){
        // 对于父进程，要用于回收僵尸进程
        while(1){
            uint32_t status;
            syscall_wait(0, &status);
        }
    }else{
        char* prog = "shell";
        syscall_exec(prog, 0, NULL);
    } 
}
