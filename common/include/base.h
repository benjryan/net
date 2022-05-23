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

global s8  min_S8  = (s8) 0x80;
global s16 min_S16 = (s16)0x8000;
global s32 min_S32 = (s32)0x80000000;
global s64 min_S64 = (s64)0x8000000000000000llu;

global s8  max_S8  = (s8) 0x7f;
global s16 max_S16 = (s16)0x7fff;
global s32 max_S32 = (s32)0x7fffffff;
global s64 max_S64 = (s64)0x7fffffffffffffffllu;

global u8  max_U8  = 0xff;
global u16 max_U16 = 0xffff;
global u32 max_U32 = 0xffffffff;
global u64 max_U64 = 0xffffffffffffffffllu;

global f32 machine_epsilon_F32 = 1.1920929e-7f;
global f32 pi_F32  = 3.14159265359f;
global f32 tau_F32 = 6.28318530718f;
global f32 e_F32 = 2.71828182846f;
global f32 gold_big_F32 = 1.61803398875f;
global f32 gold_small_F32 = 0.61803398875f;

global f64 machine_epsilon_F64 = 2.220446e-16;
global f64 pi_F64  = 3.14159265359;
global f64 tau_F64 = 6.28318530718;
global f64 e_F64 = 2.71828182846;
global f64 gold_big_F64 = 1.61803398875;
global f64 gold_small_F64 = 0.61803398875;

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

#ifdef LOGS
#   define LogError(fmt, ...) printf(("[Error] " fmt "\n"), ##__VA_ARGS__)
#   define Log(fmt, ...) printf(("[Log] " fmt "\n"), ##__VA_ARGS__)
#else
#   define LogError(fmt, ...)
#   define Log(fmt, ...)
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

function s32 str_hash(char* str);

#endif // BASE_H
