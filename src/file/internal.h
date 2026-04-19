//------------------------------------------------------------------------------
// Internal declarations for the File module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#pragma once

#include <file/file.h>

//------------------------------------------------------------------------------

typedef struct FileRoot {
    string name;
    string path;
} FileRoot;

typedef struct FileSystem {
    Arena arena;
    Array(FileRoot) roots;
} FileSystem;

extern FileSystem g_file_system;
extern bool       g_file_system_initialised;

void _file_ensure_initialised(void);

//------------------------------------------------------------------------------
// Path utilities
// These are used by the public API but are not intended to be used directly.
//------------------------------------------------------------------------------

// Simplify a path by detecting path prefixes that match a root.  The path is
// processed in place because we assert that the root name length is less than
// or equal to its path length.
void _path_simplify(string* path);
void _path_simplify_skipping_root(string* path, string skipped_root);

// Register a root using an agnostic path. When `create_if_missing` is false,
// the root is only added if the directory already exists.
bool _path_register_root(string name, string path, bool create_if_missing);

// Add a root using a host platform absolute path.
bool _path_add_host_root(string name, string platform_path);

// Add a root using a host platform absolute path only if it already exists.
bool _path_add_host_root_if_exists(string name, string platform_path);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
