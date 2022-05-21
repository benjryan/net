#include "base.h"

function Arena* make_arena(u64 size) {
    Arena* result = (Arena*)malloc(size);
    result->size = size;
    result->pos = Align(sizeof(Arena), 64);
    return result;
}

function void* arena_unaligned_push(Arena* arena, u64 size) {
    u64 pos = arena->pos + size;
    assert(pos < arena->size);
    void* result = ((u8*)arena) + arena->pos;
    arena->pos = pos;
    return result;
}

function void* arena_push(Arena* arena, u64 size) {
    u64 aligned_size = Align(size, 64);
    u64 pos = arena->pos + aligned_size;
    assert(pos < arena->size);
    void* result = ((u8*)arena) + arena->pos;
    arena->pos = pos;
    return result;
}

function void* arena_push_zero(Arena* arena, u64 size) {
    u64 aligned_size = Align(size, 64);
    void* result = arena_unaligned_push(arena, aligned_size);
    MemZero(result, aligned_size);
    return result;
}

function void arena_pop_to(Arena* arena, u64 pos) {
    assert(pos < arena->size && pos >= 0);
    arena->pos = pos;
}

function Arena_Temp arena_temp_begin(Arena* arena) {
    Arena_Temp temp = { arena, arena->pos };
    return temp;
}

function void arena_temp_end(Arena_Temp temp) {
    arena_pop_to(temp.arena, temp.pos);
}
