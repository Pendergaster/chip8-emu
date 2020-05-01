/************************************************************
 * Check license.txt in project root for license information *
 *********************************************************** */

#ifndef UTILSDEFS
#define UTILSDEFS
#include <inttypes.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef uint64_t    u64;
typedef int64_t     i64;
typedef uint32_t    u32;
typedef int32_t     i32;
typedef uint16_t    u16;
typedef int16_t     i16;
typedef uint8_t     u8;
typedef int8_t      i8;

static const u8  numeric_max_u8  = 0xFF;
static const u16 numeric_max_u16 = 0xFFFF;
static const u32 numeric_max_u32 = 0xFFFFFFFF;
static const u64 numeric_max_u64 = 0xFFFFFFFFFFFFFFFF;

#define VA_ARGS(...) , ##__VA_ARGS__

#define free(PTR) do{free((PTR));(PTR) = NULL;}while(0)

#define PTR_CAST(TYPE, VAL) (((union {typeof(VAL) src; TYPE dst;}*)(&VAL))->dst)

#define typeof __typeof__

#define BIT_CHECK(a,b) ((a & b) > 0)
#define BIT_SET(a,b) ( a |= b)
#define BIT_UNSET(a,b) (a &= ~b)

#define KILOS(NUM) (NUM * 1000)
#define MEGAS(NUM) (NUM * 1000000)
#define GIGAS(NUM) (NUM * 1000000000)

// NULL pointer should evaluate to 0
#define SIZEOF_ARRAY(ARR) (sizeof((ARR)) / sizeof(*(ARR)))
#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

// no tricks, can swap same value, hopefully optimized
#define SWAP_VALUES(FIRST, SECOND) \
    do{typeof(FIRST) temp = (FIRST); (FIRST) = (SECOND); (SECOND) = temp; }while(0)

// what is the real definition, will never know
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__) || defined(_MSC_VER)
#define WINDOWS_PLATFORM
#else
#define LINUX_PLATFORM
#endif


#endif // UTILSDEFS
