//------------------------------------------------------------------------------
// Log module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: core
//> def: _POSIX_C_SOURCE=200809L

#pragma once

#include <core/core.h>

//------------------------------------------------------------------------------

typedef struct {
    char path[512];
} Log;

bool log_append(Log* log, const void* data, usize len);
bool log_append_cstr(Log* log, cstr text);
bool log_format(Log* log, cstr fmt, ...);
bool log_formatv(Log* log, cstr fmt, va_list args);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
