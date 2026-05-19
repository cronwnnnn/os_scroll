#include "user_sys/syscall.h"

extern int32_t trigger_syscall_fork();
extern void trigger_syscall_print(char* str);
extern void trigger_syscall_exit(int32_t num);
extern int32_t trigger_syscall_exec(char* file_path, int32_t argc, char** agrv);
extern int32_t trigger_syscall_read_char();;
extern int32_t trigger_syscall_wait(int32_t pid, uint32_t* status);
extern int32_t trigger_syscall_listdir(char* dir);
extern int32_t trigger_syscall_get_ticks();
extern int32_t trigger_syscall_video_set_mode(uint32_t mode);
extern int32_t trigger_syscall_video_blit(uint8_t* buffer, uint32_t width, uint32_t height);
extern int32_t trigger_syscall_video_set_palette(uint32_t index, uint32_t r, uint32_t g, uint32_t b);
extern int32_t trigger_syscall_poll_keyboard_event(keyboard_event_t* event);
extern int32_t trigger_syscall_get_key_state(uint32_t key);

int32_t syscall_fork(){
    return trigger_syscall_fork();
}

void syscall_print(char* str) {
  return trigger_syscall_print(str);
}

void syscall_exit(int32_t exit_num) {
  return trigger_syscall_exit(exit_num);
}

int32_t syscall_exec(char* file_path, int32_t argc, char** argv){
  return trigger_syscall_exec(file_path, argc, argv);
}

int32_t syscall_read_char() {
  return trigger_syscall_read_char();
}

int32_t syscall_wait(int32_t pid, uint32_t* status) {
  return trigger_syscall_wait(pid, status);
}

int32_t syscall_listdir(char* dir){
  return trigger_syscall_listdir(dir);
}

int32_t syscall_get_ticks() {
  return trigger_syscall_get_ticks();
}

int32_t syscall_video_set_mode(uint32_t mode) {
  return trigger_syscall_video_set_mode(mode);
}

int32_t syscall_video_blit(uint8_t* buffer, uint32_t width, uint32_t height) {
  return trigger_syscall_video_blit(buffer, width, height);
}

int32_t syscall_video_set_palette(uint32_t index, uint32_t r, uint32_t g, uint32_t b) {
  return trigger_syscall_video_set_palette(index, r, g, b);
}

int32_t syscall_poll_keyboard_event(keyboard_event_t* event) {
  return trigger_syscall_poll_keyboard_event(event);
}

int32_t syscall_get_key_state(uint32_t key) {
  return trigger_syscall_get_key_state(key);
}
