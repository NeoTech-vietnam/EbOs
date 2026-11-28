#ifndef TEST_STD_TYPES_H
#define TEST_STD_TYPES_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32_t sint32;
typedef uint8 boolean;
#ifndef STATUSTYPEDEFINED
#define STATUSTYPEDEFINED
typedef unsigned char StatusType;
#define E_OK 0x00U
#endif
#define NULL_PTR ((void *)0)
#define TRUE 1U
#define FALSE 0U
#endif
