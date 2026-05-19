#ifndef SYSCALL_SYSCALL_H
#define SSYSCALL_SYSCALL_H
#include "common/types.h"

int32_t syscall_fork();
void syscall_print(char* str);
void syscall_exit(int32_t num);
int32_t syscall_exec(char* file_path, int32_t argc, char** argv);
int32_t syscall_read_char();
int32_t syscall_wait(int32_t pid, uint32_t* status);
int32_t syscall_listdir(char* dir);

int32_t syscall_get_ticks();
int32_t syscall_video_set_mode(uint32_t mode);
int32_t syscall_video_blit(uint8_t* buffer, uint32_t width, uint32_t height);
int32_t syscall_video_set_palette(uint32_t index, uint32_t r, uint32_t g, uint32_t b);

#endif
