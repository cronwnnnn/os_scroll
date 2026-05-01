#include "syscall/syscall.h"
#include "common/types.h"

int main(int32_t argc, char** agrv){
    syscall_print("successfully load user file!!!!\n");
    int32_t pid = syscall_fork();
    if(pid < 0){
        syscall_print("fork failed\n");
    }else if(pid > 0){
        while(1){}
    }else{
        char* prog = "shell";
        syscall_exec(prog, 0, NULL);
    } 
}
