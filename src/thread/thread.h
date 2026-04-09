//------------------------------------------------------------------------------
// Threads and channels module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: core
//> def: _POSIX_C_SOURCE=199309L

#pragma once

#include <core/core.h>

//------------------------------------------------------------------------------

#if OS_WINDOWS
typedef HANDLE Thread;
#else
typedef pthread_t Thread;
#endif

//------------------------------------------------------------------------------
// Thread API

bool thread_create(Thread* thread, void* (*func)(void*), void* arg);
void thread_join(Thread* thread);
void thread_yield();
void thread_sleep_ms(u32 milliseconds);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
