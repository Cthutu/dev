//------------------------------------------------------------------------------
// Main entry point
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <core/core.h>

extern Mutex g_kore_output_mutex;
Arena        g_temp_arena;
u64          g_temp_arena_in_use = 0;

//------------------------------------------------------------------------------

int run(int argc, char** argv);

#ifndef TEST
int main(int argc, char** argv)
{
    mutex_init(&g_kore_output_mutex);

#if OS_WINDOWS
    UINT old_cp = GetConsoleCP();
    SetConsoleCP(CP_UTF8);
    UINT old_output_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
#endif // OS_WINDOWS

    int result = run(argc, argv);

#if OS_WINDOWS
    SetConsoleCP(old_cp);
    SetConsoleOutputCP(old_output_cp);
#endif // OS_WINDOWS

#if CONFIG_DEBUG
    mem_print_leaks();
#endif // CONFIG_DEBUG
    mutex_done(&g_kore_output_mutex);
    return result;
}
#endif

void kill(cstr format, ...)
{
    va_list args;
    va_start(args, format);
    eprv(format, args);
    va_end(args);
    epr("\n");
    abort();
}

void temp_arena_init()
{
    if (!g_temp_arena_in_use) {
        arena_init(&g_temp_arena, .reserved_size = 4096, .grow_rate = 1);
    }
    ++g_temp_arena_in_use;
}

void temp_arena_done()
{
    ASSERT(g_temp_arena_in_use > 0, "Temporary arena is not in use");

    --g_temp_arena_in_use;
    if (!g_temp_arena_in_use) {
        arena_done(&g_temp_arena);
    }
}

Arena* temp_arena()
{
    ASSERT(g_temp_arena_in_use, "Temporary arena is not in use");
    return &g_temp_arena;
}

void temp_arena_reset()
{
    ASSERT(g_temp_arena_in_use, "Temporary arena is not in use");
    arena_reset(&g_temp_arena);
}
