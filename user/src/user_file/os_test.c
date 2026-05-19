#include "common/types.h"
#include "user_sys/syscall.h"
#include "user_sys/stdlib.h"
#include "user_sys/user_io.h"

static int32_t total_tests = 0;
static int32_t failed_tests = 0;
static int32_t shared_value = 7;

static void busy_delay(void) {
    volatile int32_t i = 2000000;
    while (i-- > 0) {
    }
}

static void report_result(char* name, int32_t pass) {
    total_tests++;
    if (pass) {
        printf("[PASS] %s\n", name);
    } else {
        failed_tests++;
        printf("[FAIL] %s\n", name);
    }
}

static void exit_with(int32_t code) {
    syscall_exit(code);
    while (1) {
    }
}

static int32_t run_wait_child(int32_t exit_code) {
    int32_t pid = syscall_fork();
    if (pid < 0) {
        return 0;
    }
    if (pid == 0) {
        exit_with(exit_code);
    }

    uint32_t status = 0;
    int32_t ret = syscall_wait(pid, &status);
    return ret == 0 && status == (uint32_t)exit_code;
}

static int32_t run_wait_any_children(void) {
    int32_t expected_sum = 0;

    for (int32_t i = 0; i < 3; i++) {
        int32_t exit_code = 31 + i;
        int32_t pid = syscall_fork();
        if (pid < 0) {
            return 0;
        }
        if (pid == 0) {
            busy_delay();
            exit_with(exit_code);
        }
        expected_sum += exit_code;
    }

    int32_t got_sum = 0;
    for (int32_t i = 0; i < 3; i++) {
        uint32_t status = 0;
        if (syscall_wait(0, &status) != 0) {
            return 0;
        }
        got_sum += (int32_t)status;
    }

    return got_sum == expected_sum;
}

static int32_t run_memory_isolation(void) {
    int32_t local_value = 11;
    shared_value = 7;

    int32_t pid = syscall_fork();
    if (pid < 0) {
        return 0;
    }
    if (pid == 0) {
        local_value = 99;
        shared_value = 123;
        busy_delay();
        exit_with(local_value + shared_value);
    }

    busy_delay();
    uint32_t status = 0;
    if (syscall_wait(pid, &status) != 0) {
        return 0;
    }

    return local_value == 11 && shared_value == 7 && status == 222;
}

static int32_t run_exec_probe(void) {
    int32_t pid = syscall_fork();
    if (pid < 0) {
        return 0;
    }
    if (pid == 0) {
        char* argv[] = {"os_test", "--exec-probe", "alpha", "beta", NULL};
        int32_t ret = syscall_exec(argv[0], 4, argv);
        exit_with(100 + (-ret));
    }

    uint32_t status = 0;
    if (syscall_wait(pid, &status) != 0) {
        return 0;
    }
    return status == 44;
}

static int32_t run_stack_touch(void) {
    volatile uint32_t values[128];
    uint32_t sum = 0;

    for (uint32_t i = 0; i < 128; i++) {
        values[i] = i ^ 0x55aa55aa;
    }
    for (uint32_t i = 0; i < 128; i++) {
        sum += values[i] ^ 0x55aa55aa;
    }

    return sum == 8128;
}

static void run_exec_probe_mode(int32_t argc, char** argv) {
    int32_t pass = argc == 4
        && strcmp(argv[0], "os_test") == 0
        && strcmp(argv[1], "--exec-probe") == 0
        && strcmp(argv[2], "alpha") == 0
        && strcmp(argv[3], "beta") == 0;
    exit_with(pass ? 44 : 45);
}

int main(int32_t argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--exec-probe") == 0) {
        run_exec_probe_mode(argc, argv);
    }

    printf("\n==== os_scroll user/kernel smoke tests ====\n");

    report_result("print syscall and user printf", 1);
    report_result("wait with no child returns -1", syscall_wait(0, NULL) == -1);
    report_result("fork + wait(pid) + exit status", run_wait_child(21));
    report_result("wait invalid pid returns -1", syscall_wait(12345, NULL) == -1);
    report_result("wait(0) reaps multiple children", run_wait_any_children());
    report_result("fork keeps parent memory isolated", run_memory_isolation());
    report_result("exec preserves argv and exit status", run_exec_probe());
    report_result("user stack read/write", run_stack_touch());

    printf("==== os_scroll tests: %d total, %d failed ====\n", total_tests, failed_tests);
    syscall_listdir(".");

    return failed_tests == 0 ? 0 : 1;
}
