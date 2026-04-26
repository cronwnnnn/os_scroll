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

    uint32_t id;
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

    // child pid that is waiting
    uint32_t waiting_child_pid;

    // waiting thread
    struct linked_list_node* waiting_thread_node;

    // page directory
    page_directory_t page_dir;
    yieldlock_t page_dir_lock;

    // lock to protect this struct
    yieldlock_t lock;
};
typedef struct process_struct pcb_t;

void init_process_manager();
tcb_t* create_new_user_thread(pcb_t* process, char* name, void* user_function, uint32_t argc, char** argv);
tcb_t* create_new_kernel_thread(pcb_t* process, char* name, void* function);
pcb_t* create_and_add_process(char* name, uint8_t is_kernel_process);
int32_t process_fork();

#endif