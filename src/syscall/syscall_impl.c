#include "interrupt/interrupt.h"
#include "syscall/syscall_impl.h"
#include "common/secure.h"
#include "task/process.h"
#include "monitor/monitor.h"

static int32_t syscall_exit_impl(int32_t exit_num){
    Panic("this system call not exist");
    return -1;
}

static int32_t syscall_fork_impl(){
    return process_fork();
}

static int32_t syscall_exec_impl(char* file_path, uint32_t argc, char** argv){
    Panic("this system call not exist");
    return -1;
}

static int32_t syscall_print_impl(char* str) {
  monitor_print(str);
  return 0;
}




int32_t syscall_handler(isr_params_t isr_params){
    // eax中存的是系统调用号
    int32_t syscall_num = isr_params.eax;
    switch (syscall_num){
        case SYSCALL_EXIT_NUM:
            return syscall_exit_impl((int32_t)isr_params.ecx); 
        case SYSCALL_FORK_NUM:
            return syscall_fork_impl();
        case SYSCALL_EXEC_NUM:
            return syscall_exec_impl((char*)isr_params.ecx, isr_params.edx, (char**)isr_params.ebx);
        case SYSCALL_PRINT_NUM:
            return syscall_print_impl((char*)isr_params.ecx);

        default:
            Panic("wrong syscall num !!!");
        }
    return -1;
}