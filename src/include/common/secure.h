#ifndef COMMON_SECURE_H
#define COMMON_SECURE_H

void Panic(const char* message, ...);
void panic_spin(char* filename, int line, const char* func, const char* condition);
#define Assert(condition) \
    do { \
        if (!(condition)) { \
            panic_spin(__FILE__, __LINE__, __func__, #condition); \
        } \
    } while (0)


#endif // COMMON_SECURE_H