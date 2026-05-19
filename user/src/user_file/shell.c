#include "common/types.h"
#include "user_sys/syscall.h"
#include "user_sys/stdlib.h"
#include "user_sys/user_io.h"

void parse_cmd(char* cmd, char** argv);


static void run_program(char** argv){
    int32_t argc = 0;
    while(argv[argc]){
        argc++;
    }
    int32_t pid = syscall_fork();
    if(pid < 0){
        printf("fork failed!\n");
    }else if(pid > 0){
        uint32_t status;
        syscall_wait(pid, &status);
    }else{
        int32_t ret = syscall_exec(argv[0], argc, argv);
        syscall_exit(ret);
    }
    return;
}



int main(int32_t argc, char** agrv){
    char command_line[1024];
    // 这是一个存着64个char*类型指针打的数组，每个数组的值为cmd_line数组的地址某个字符开头
    char *argv[64];

    while (1) {
        // 1. 打印提示符
        printf("my_os_shell> ");
        
        // 2. 读取用户输入
        readline(command_line);
        
        // 3. 解析输入成参数数组
        parse_cmd(command_line, argv);
        
        // 4. 判断是否为空输入，为空则跳过
        if (argv[0] == NULL) continue;
        
        // 同步运行的
        run_program(argv);
    }
    return 0;
}



void parse_cmd(char* cmd, char** argv){
    int argc = 0;
    char *ptr = cmd;
    while(*ptr != '\0'){
        while(*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r'){
            ptr++;
        }
        if(*ptr == '\0'){
            break;
        }

        // 记录当前参数的地址
        argv[argc] = ptr;
        argc++;
        // 找到下一个由空格或者其他中断符隔开的参数
        while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') {
            ptr++;
        }
        // 如果不是cmd的结尾，就截断，分割字符,找下一个参数
        // 若是结尾，则再下一次while时就直接退出了
        if(*ptr != '\0'){
            *ptr = '\0';
            ptr++;
        }
    }

    //  最后一个argv必须为null,保证第二次写入的时候不会被第一次的字符影响
    argv[argc] = NULL;
}