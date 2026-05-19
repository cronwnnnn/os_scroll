#include "interrupt/interrupt.h"
#include "syscall/syscall_impl.h"
#include "common/secure.h"
#include "task/process.h"
#include "monitor/monitor.h"
#include "driver/keyboard.h"
#include "driver/video.h"
#include "fs/vfs.h"
#include "interrupt/time.h"


static int32_t syscall_exit_impl(int32_t exit_num){
    process_exit(exit_num);
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

static int32_t syscall_wait_impl(int32_t pid, uint32_t* status) {
  return process_wait(pid, status);
}

static int32_t syscall_listdir_impl(char* dir){
    return list_dir(dir);
}

static int32_t syscall_get_ticks_impl() {
    return (int32_t)getTick();
}

static int32_t syscall_video_set_mode_impl(uint32_t mode) {
    video_set_mode(mode);
    return 0;
}

static int32_t syscall_video_blit_impl(uint8_t* buffer, uint32_t width, uint32_t height) {
    return video_blit(buffer, width, height);
}

static int32_t syscall_video_set_palette_impl(uint32_t index, uint32_t r, uint32_t g, uint32_t b) {
    video_set_palette(index, r, g, b);
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
        case SYSCALL_READ_CHAR_NUM:
            return syscall_read_char_impl();
        case SYSCALL_WAIT_NUM:
            return syscall_wait_impl(isr_params.ecx, (uint32_t*)isr_params.edx);
        case SYSCALL_LISTDIR_NUM:
            return syscall_listdir_impl((char*)isr_params.ecx);
        case SYSCALL_GET_TICKS_NUM:
            return syscall_get_ticks_impl();
        case SYSCALL_VIDEO_SET_MODE_NUM:
            return syscall_video_set_mode_impl(isr_params.ecx);
        case SYSCALL_VIDEO_BLIT_NUM:
            return syscall_video_blit_impl((uint8_t*)isr_params.ecx, isr_params.edx, isr_params.ebx);
        case SYSCALL_VIDEO_SET_PALETTE_NUM:
            return syscall_video_set_palette_impl(isr_params.ecx, isr_params.edx, isr_params.ebx, isr_params.esi);


        default:
            Panic("wrong syscall num !!!");
        }
    return -1;
}