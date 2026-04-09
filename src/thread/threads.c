//------------------------------------------------------------------------------
// Threads API
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <thread/thread.h>

#if OS_POSIX
#    include <sched.h>
#    include <time.h>
#endif

//------------------------------------------------------------------------------
// Windows implementation

#if CS_OS_WINDOWS

bool thread_create(CS_Thread* thread, void (*func)(void*), void* arg)
{
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return *thread != NULL;
}

void thread_join(CS_Thread* thread)
{
    WaitForSingleObject(*thread, INFINITE);
    CloseHandle(*thread);
    *thread = NULL;
}

void thread_yield() { SwitchToThread(); }

void thread_sleep_ms(u32 milliseconds) { Sleep(milliseconds); }

Thread thread_this() { return GetCurrentThread(); }

//------------------------------------------------------------------------------
// POSIX implementation

#elif OS_POSIX

bool thread_create(Thread* thread, void* (*func)(void*), void* arg)
{
    return pthread_create(thread, NULL, (void* (*)(void*))func, arg) == 0;
}

void thread_join(Thread* thread)
{
    pthread_join(*thread, NULL);
    *thread = 0;
}

void thread_yield() { sched_yield(); }

void thread_sleep_ms(u32 milliseconds)
{
    struct timespec ts;
    ts.tv_sec  = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

Thread thread_this() { return pthread_self(); }

//------------------------------------------------------------------------------

#else
#    error "Thread API not implemented for this platform"
#endif

//------------------------------------------------------------------------------
// Cancel tokens

void thread_cancel(CancelToken* token) { *token = true; }

bool thread_is_cancelled(CancelToken* token) { return *token; }

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
