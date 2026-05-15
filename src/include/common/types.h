#ifndef COMMON_TYPE_H
#define COMMON_TYPE_H

#define nullptr ((void*)0)
#ifndef NULL
#define NULL ((void*)0)
#endif
#define true 1
#define false 0

// 防止与宿主机编译器的 <stdint.h> 或 <stddef.h> 发生冲突
#ifndef _STDINT_H
#ifndef _STDINT_H_
typedef void* type_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef char int8_t;
typedef unsigned char uint8_t;
#endif
#endif

typedef unsigned char bool;

#ifndef _SIZE_T
#ifndef _SIZE_T_DEFINED
typedef uint32_t size_t;
#endif
#endif

#endif // COMMON_TYPE_H