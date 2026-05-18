void dummy_delay() {
    // 这里的数字需要足够大，确保在这个循环跑完期间，
    // 时钟中断（比如你设置的是 10ms 触发一次）至少会发生一次或多次。
    // 如果系统比较慢，可以把数字调小点；如果用的是比较快的虚拟机，可能要写到 10000000。
    volatile int i = 50000000; 
    while (i > 0) {
        i--;
    }
}

int global_var = 100;
void user_mode_fork_test() {
    syscall_print("===================\n");
    syscall_print("User Mode Fork Test Start!\n");
    
    uint32_t* ptr = (uint32_t*)0xF2000000;
    *ptr = 1;

    int local_var = 10; 
    
    int32_t pid = syscall_fork();

    if (pid < 0) {
        syscall_print("Fork Failed!\n");
        while(1);
    } 
    else if (pid == 0) {
        // --- 子进程 ---
        syscall_print("[Child] I am the child process.\n");
        
        local_var = 20;    
        global_var = 200;  
        
        syscall_print("[Child] Modifying variables: local=20, global=200\n");
        
        // 刻意拖延时间，在这个期间，时钟中断肯定会发生，
        // CPU 会被强行抢占，切换回父进程去检查变量是否被污染。
        syscall_print("[Child] Delaying to force a preemption...\n");
        dummy_delay(); 
        
        syscall_print("[Child] Execution finished.\n");
        while (1) { dummy_delay(); } // 挂起
    } 
    else {
        // --- 父进程 ---
        syscall_print("[Parent] I am the parent. My child's PID is > 0.\n");
        syscall_print("[Parent] Waiting for child to modify variables...\n");
        
        // 父进程这里不着急检查，先跑一个漫长的延时循环。
        // 这期间必然会被时钟中断打断，切换到刚 fork 出来的子进程那里去。
        // 等子进程修改完变量，并同样因为时钟中断被挂起后，又会切回到父进程继续走。
        dummy_delay();
        dummy_delay(); // 跑两次确保子进程已经改过变量了

        // 验证内存隔离
        if (local_var == 10 && global_var == 200) {
            syscall_print("[Parent] Test PASS: Memory is isolated!\n");
        } else {
            syscall_print("[Parent] Test FAIL: Variables were overwritten by child!\n");
        }
        
        syscall_print("[Parent] Execution finished.\n");
        while (1) { dummy_delay(); } // 挂起
    }
}