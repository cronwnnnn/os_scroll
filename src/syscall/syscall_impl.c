#include "interrupt/interrupt.h"
#include "syscall/syscall_impl.h"
#include "common/secure.h"
#include "task/process.h"
#include "monitor/monitor.h"
#include "driver/keyboard.h"

static int32_t syscall_exit_impl(int32_t exit_num){
    Panic("this system call not exist");
    return -1;
}

static int32_t syscall_fork_impl(){
    return process_fork();
}

static int32_t syscall_exec_impl(char* file_path, int32_t argc, char** argv){
    process_exec(file_path, argc, argv);
    return -1;
}

static int32_t syscall_print_impl(char* str) {
    monitor_print(str);
    return 0;
}

static int32_t syscall_read_char_impl() {
    return read_keyboard_char();
}

static int32_t syscall_wait_impl(uint32_t pid, uint32_t* status) {
  return process_wait(pid, status);
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
        case SYSCALL_READ_CHAR_NUM:
            return syscall_read_char_impl();
        case SYSCALL_WAIT_NUM:
            return syscall_wait_impl(isr_params.ecx, (uint32_t*)isr_params.edx);


        default:
            Panic("wrong syscall num !!!");
        }
    return -1;
}