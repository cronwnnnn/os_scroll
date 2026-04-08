#ifndef COMMON_SECURE_H
#define COMMON_SECURE_H

void Panic(const char* message, ...);
#define Assert(x) if (!(x)) Panic("Assertion failed: " #x)

#endif // COMMON_SECURE_H