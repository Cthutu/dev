//------------------------------------------------------------------------------
// Time API implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <core/core.h>

//------------------------------------------------------------------------------

#if OS_WINDOWS

static u64 _time_frequency(void)
{
    local_persist u64 frequency = 0;
    if (frequency == 0) {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        frequency = (u64)value.QuadPart;
    }
    return frequency;
}

static u64 _time_qpc_to_ns(u64 counter)
{
    return (counter * 1000000000ull) / _time_frequency();
}

TimePoint time_now(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (TimePoint)_time_qpc_to_ns((u64)counter.QuadPart);
}

TimeDuration time_elapsed(TimePoint start, TimePoint end)
{
    return (TimeDuration)(end - start);
}

TimePoint time_add_duration(TimePoint time, TimeDuration duration)
{
    return time + duration;
}

void time_sleep_ms(u32 milliseconds) { Sleep(milliseconds); }

u64 time_duration_to_secs(TimeDuration duration) { return duration / 1000000000ull; }

u64 time_duration_to_ms(TimeDuration duration) { return duration / 1000000ull; }

u64 time_duration_to_us(TimeDuration duration) { return duration / 1000ull; }

u64 time_duration_to_ns(TimeDuration duration) { return duration; }

f64 time_secs(TimeDuration duration) { return (f64)duration / 1000000000.0; }

TimeDuration time_from_secs(u64 seconds) { return seconds * 1000000000ull; }

TimeDuration time_from_ms(u64 milliseconds) { return milliseconds * 1000000ull; }

TimeDuration time_from_us(u64 microseconds) { return microseconds * 1000ull; }

TimeDuration time_from_ns(u64 nanoseconds) { return nanoseconds; }

#elif OS_POSIX

static inline TimePoint _time_now_raw(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TimePoint)ts.tv_sec * 1000000000ull + (TimePoint)ts.tv_nsec;
}

TimePoint time_now(void) { return _time_now_raw(); }

TimeDuration time_elapsed(TimePoint start, TimePoint end)
{
    return (TimeDuration)(end - start);
}

TimePoint time_add_duration(TimePoint time, TimeDuration duration)
{
    return time + duration;
}

void time_sleep_ms(u32 milliseconds)
{
    struct timespec req;
    req.tv_sec  = (time_t)(milliseconds / 1000);
    req.tv_nsec = (long)((milliseconds % 1000) * 1000000ul);
    nanosleep(&req, NULL);
}

u64 time_duration_to_secs(TimeDuration duration)
{
    return duration / 1000000000ull;
}

u64 time_duration_to_ms(TimeDuration duration) { return duration / 1000000ull; }

u64 time_duration_to_us(TimeDuration duration) { return duration / 1000ull; }

u64 time_duration_to_ns(TimeDuration duration) { return duration; }

f64 time_secs(TimeDuration duration) { return (f64)duration / 1000000000.0; }

TimeDuration time_from_secs(u64 seconds) { return seconds * 1000000000ull; }

TimeDuration time_from_ms(u64 milliseconds)
{
    return milliseconds * 1000000ull;
}

TimeDuration time_from_us(u64 microseconds) { return microseconds * 1000ull; }

TimeDuration time_from_ns(u64 nanoseconds) { return nanoseconds; }

#endif // OS_WINDOWS
