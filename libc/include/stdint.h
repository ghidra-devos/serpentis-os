#ifndef STDINT_H
#define STDINT_H

typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;

#define INT32_MAX 2147483647
#define INT32_MIN (-INT32_MAX - 1)
#define UINT32_MAX 4294967295U
#define INT64_MAX 9223372036854775807L
#define INT64_MIN (-INT64_MAX - 1)
#define UINT64_MAX 18446744073709551615UL

#endif
