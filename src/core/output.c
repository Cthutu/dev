//------------------------------------------------------------------------------
// Output implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <core/core.h>

#include <stdio.h>

Mutex g_kore_output_mutex;

//------------------------------------------------------------------------------

#if OS_WINDOWS
global_variable INIT_ONCE g_kore_output_mutex_once = INIT_ONCE_STATIC_INIT;

internal BOOL CALLBACK _kore_output_mutex_once_init(PINIT_ONCE init_once,
                                                    PVOID      parameter,
                                                    PVOID*     context)
{
    UNUSED(init_once);
    UNUSED(parameter);
    UNUSED(context);
    mutex_init(&g_kore_output_mutex);
    return TRUE;
}

internal void _kore_output_mutex_ensure_init(void)
{
    InitOnceExecuteOnce(
        &g_kore_output_mutex_once, _kore_output_mutex_once_init, NULL, NULL);
}
#else
global_variable pthread_once_t g_kore_output_mutex_once = PTHREAD_ONCE_INIT;

internal void _kore_output_mutex_once_init(void)
{
    mutex_init(&g_kore_output_mutex);
}

internal void _kore_output_mutex_ensure_init(void)
{
    pthread_once(&g_kore_output_mutex_once, _kore_output_mutex_once_init);
}
#endif

//------------------------------------------------------------------------------

internal cstr _format_output(cstr format, va_list args, usize* out_size)
{
    thread_local local_persist Array(char) print_buffer = NULL;

    // Get the size of the formatted string.
    va_list args_copy;
    va_copy(args_copy, args);
    *out_size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    // Allocate or reallocate the print buffer if necessary.
    array_requires_capacity(print_buffer, *out_size + 1);
    array_leak(print_buffer); // Prevent detection in leaks

    // Format the string into the buffer.
    vsnprintf(print_buffer, *out_size + 1, format, args);

    return print_buffer;
}

#if OS_POSIX

internal void fprv(int fd, cstr format, va_list args)
{
    usize size;
    _kore_output_mutex_ensure_init();
    mutex_lock(&g_kore_output_mutex);
    cstr output = _format_output(format, args, &size);
    write(fd, output, size);
    mutex_unlock(&g_kore_output_mutex);
}

void prv(cstr format, va_list args) { fprv(STDOUT_FILENO, format, args); }
void eprv(cstr format, va_list args) { fprv(STDERR_FILENO, format, args); }

#elif defined(OS_WINDOWS)

internal void fprv(HANDLE handle, cstr format, va_list args)
{
    usize size;
    _kore_output_mutex_ensure_init();
    mutex_lock(&g_kore_output_mutex);
    cstr output = _format_output(format, args, &size);

    // Write to the file handle.
    DWORD console_mode;
    BOOL  is_console = GetConsoleMode(handle, &console_mode);

    DWORD bytes_written;
    if (is_console) {
        WriteConsoleA(handle, output, size, &bytes_written, NULL);
    } else {
        WriteFile(handle, output, size, &bytes_written, NULL);
    }
    mutex_unlock(&g_kore_output_mutex);
}

void prv(cstr format, va_list args)
{
    fprv(GetStdHandle(STD_OUTPUT_HANDLE), format, args);
}

void eprv(cstr format, va_list args)
{
    fprv(GetStdHandle(STD_ERROR_HANDLE), format, args);
}

#else
#    error "Output functions not implemented for this OS."

#endif // OS_POSIX

void pr(cstr format, ...)
{
    va_list args;
    va_start(args, format);
    prv(format, args);
    va_end(args);
}

void prn(cstr format, ...)
{
    va_list args;
    va_start(args, format);
    prv(format, args);
    pr("\n");
    va_end(args);
}

void epr(cstr format, ...)
{
    va_list args;
    va_start(args, format);
    eprv(format, args);
    va_end(args);
}

void eprn(cstr format, ...)
{
    va_list args;
    va_start(args, format);
    eprv(format, args);
    epr("\n");
    va_end(args);
}
