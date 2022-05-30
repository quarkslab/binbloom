#pragma once

#define STR_MIN_SIZE 8
#define MAX_MEM_AMOUNT 4000000000
#define DEFAULT_BASE_ADDRESS 0xffffffffffffffff
#define MAX_STRUCT_MEMBERS 12

typedef enum {
    ARCH_32,
    ARCH_64,
} arch_t;

typedef enum {
    ENDIAN_UNKNOWN,
    ENDIAN_LE,
    ENDIAN_BE
} endianness_t;
