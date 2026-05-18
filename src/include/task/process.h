#ifndef TASK_PROCESS_H
#define TASK_PROCESS_H

#include "utils/hash_table.h"
#include "utils/bit_map.h"
#include "sync/yieldlock.h"
#include "mem/page.h"
#include "task/thread.h"


#define USER_STACK_TOP   0xBFC00000  // 0xC0000000 - 4MB
#define USER_STACK_SIZE  65536       // 64KB
#define USER_PRCOESS_THREDS_MAX  4096

#define PROCESS_NUM 1024
#define MAX_PROCESS_NUM 16384

struct task_control_block;
typedef struct task_control_block tcb_t;


enum process_status {
  PROCESS_NORMAL,
  PROCESS_EXIT,
  PROCESS_EXIT_ZOMBIE
};

struct process_struct {

    int32_t id;
    char name[32];

    struct process_struct* parent;

    enum process_status status;

    // tid -> threads
    hash_table_t threads;

    // allocate user space thread for threads
    bitmap_t user_thread_stack_indexes;

    // is kernel process?
    uint8_t is_kernel_process;

    // exit code
    int32_t exit_code;

    // children processes
    hash_table_t children_processes;

    // exit children processes
    hash_table_t exit_children_processes;

    // 有哪些线程在等待自己死亡
    struct wait_node* exit_wait_queue;
    // 当前进程中等待任何一个的线程
    struct wait_node* wait_any_exit_queue;

    // Number of threads currently blocked or sleeping in process_wait().
    uint32_t waiting_threads_count;

    // page directory
    page_directory_t page_dir;
    yieldlock_t page_dir_lock;

    // lock to protect this struct
    yieldlock_t lock;
};
typedef struct process_struct pcb_t;

struct wait_node {
    thread_node_t* task; // 真正的主角：指向那个在睡觉的线程的指针
    int32_t target_pid;         // 悬赏条件：你在等谁？
    struct wait_node* next;   // 链表指针
};
typedef struct wait_node wait_node_t;

void init_process_manager();
tcb_t* create_new_user_thread(pcb_t* process, char* name, void* user_function, int32_t argc, char** argv);
tcb_t* create_new_kernel_thread(pcb_t* process, char* name, void* function);
pcb_t* create_and_add_process(char* name, uint8_t is_kernel_process);
int32_t process_fork();
void destroy_process(pcb_t* process);
int32_t process_exec(char* path, int32_t argc, char* argv[]);
int32_t process_wait(int32_t pid, uint32_t* status);
void process_exit(int32_t exit_code);

#endif