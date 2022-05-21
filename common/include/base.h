#ifndef BASE_H
#define BASE_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>

#define Stmnt(S) do{ S }while(0)

#if !defined(AssertBreak)
# define AssertBreak() (*(volatile int*)0 = 0)
#endif

#if ENABLE_ASSERT
# define Assert(c) Stmnt( if (!(c)){ AssertBreak(); } )
#else
# define Assert(c)
#endif

#define StaticAssert(c,l) typedef u8 Glue(l,__LINE__) [(c)?1:-1]
#define Stringify_(S) #S
#define Stringify(S) Stringify_(S)
#define Glue_(A,B) A##B
#define Glue(A,B) Glue_(A,B)

#define ArrayCount(a) (sizeof(a)/sizeof(*(a)))

#include <string.h>
#define MemZero(p,z) memset((p), 0, (z))
#define MemZeroStruct(p) MemZero((p), sizeof(*(p)))
#define MemZeroArray(p)  MemZero((p), sizeof(p))
#define MemZeroTyped(p,c) MemZero((p), sizeof(*(p))*(c))

#define global static
#define local static
#define function static

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef s8 b8;
typedef s16 b16;
typedef s32 b32;
typedef s64 b64;
typedef float f32;
typedef double f64;

#define TRUE 1
#define FALSE 0

#define Align(a, x) (x + a - 1) & ~(a - 1)

typedef struct {
    u64 size;
    u64 pos;
} Arena;

typedef struct {
    Arena* arena;
    u64 pos;
} Arena_Temp;

#ifdef NDEBUG
#   define LogError(fmt, ...)
#   define Log(fmt, ...)
#else
#   define LogError(fmt, ...) printf(("[Error] " fmt "\n"), ##__VA_ARGS__)
#   define Log(fmt, ...) printf(("[Log] " fmt "\n"), ##__VA_ARGS__)
#endif

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)
#define GB(x) ((x) << 30)
#define TB(x) ((x) << 40)

function Arena* make_arena(u64 size);
function void* arena_unaligned_push(Arena* arena, u64 size);
function void* arena_push(Arena* arena, u64 size);
function void* arena_push_zero(Arena* arena, u64 size);
function void arena_pop_to(Arena* arena, u64 pos);
function Arena_Temp arena_temp_begin(Arena* arena);
function void arena_temp_end(Arena_Temp temp);

#endif // BASE_H