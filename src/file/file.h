//------------------------------------------------------------------------------
// File/Path module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: core
//> def: _POSIX_C_SOURCE=200809L
//> def: _GNU_SOURCE

#pragma once

#include <core/core.h>

//------------------------------------------------------------------------------
// Paths
//
// Paths are represented in a platform agnostic way in this format:
//
//  root:/path/to/file.ext
//
// The root can be one of the following:
//
//  - `home`: user's home directory
//  - `data`: the data directory.  In debug mode, this is the data directory,
//    which is located in the project directory at the location.
//    `data/<project-name>`.  In release mode, this is the directory where the
//    executable is located.
//  - `temp`: the temporary directory.
//  - `cfg`: the configuration directory to store files created by your app.
//  - `sys`: the root directory.  On Windows that has drive letters, they can be
//    represented by `sys:/<drive letter>`.  All files can be converted to a
//    `sys` path.
//  - `app`: The directory where the executable is
//
// Paths are converted to platform specific paths when they are used.  For
// example, on Windows, the path `home:/file.txt` would be converted to
// `C:\Users\<username>\file.txt`.
//
// New roots can be created that point to directories.
//
// Some rules about these paths:
//  - Paths must be absolute.  Relative paths are not allowed.
//  - Paths must use forward slashes (`/`) as separators, even on Windows.
//  - Paths must not contain `.` or `..` components.
//  - Paths must not contain duplicate slashes (`//`).
//  - Paths must not end with a slash (`/`), unless the path is the root itself
//    (e.g. `home:/`).
//
// `file_init` must be called to use the path API.  It does not register any
// named roots automatically.  Roots are registered explicitly with the
// `file_add_*_root()` helpers.
//------------------------------------------------------------------------------

// Create a path from a platform-specific path.  The resulting path is stored
// in the given arena and the return values references it.
string path_from_platform(string platform_path, Arena* arena);

// Convert a path to a platform-specific path.  The resulting path is stored in
// the given arena and the return value references it.
string path_to_platform(string path, Arena* arena);

// Return true if the given UTF-8 character is a valid path character.
bool path_char_is_valid(u32 codepoint);

// Return true if the path is a valid path of the form
// `<root>:/path/to/filename`.
bool path_is_valid(string path);

// Add a new root in terms of a path.  Both strings are copied into the file
// system's internal arena.
//
// NOTE: the path given must be equal or longer than the name of the root.  This
// function will assert this.  The reason for this is that when paths are
// simplified, they are done in place.
//
// If the directory doesn't exists and cannot be created, this function returns
// false.
bool path_add_root(string name, string path);

// Get the root of a path.  The return value references the root in the given
// path.
string path_get_root(string path);

// Get the directory of a path.  The return value references the directory in
// the given path, excluding the file name.
string path_get_parent(string path);

// Get the file name of a path.  The return value references the file name in
// the given path, excluding the directory.
string path_get_filename(string path);

// Get the file extension of a path.  The return value references the file
// extension in the given path, excluding the file name.  If the file has no
// extension an empty string is returned.  The dot is included in the string.
string path_get_extension(string path);

// Get the stem of a file-name.  This the same as `path_get_filename` but
// without the extension.
string path_get_stem(string path);

// Check if a path is valid.  A valid path is an absolute path that follows the
// rules outlined above.
bool path_is_valid(string path);

// Check if a path exists on the file system.
bool path_exists(string path);

// Check if a path is a directory on the file system.
bool path_is_directory(string path);

// Check if a path is a file on the file system.
bool path_is_file(string path);

// Create a directory at the given path.  If the directory already exists, this
// does nothing.  If the parent directory does not exist, it is created as well.
bool path_create_directory(string path);

// Delete a file or directory at the given path.  If the path is a directory,
// it is deleted recursively.  If the path does not exist, this does nothing.
bool path_delete(string path);

// Get the size of a file at the given path.  If the path does not exist or is
// not a file, this returns false.
bool path_get_file_size(string path, u64* size);

// Get the last modified time of a file at the given path.  If the path does not
// exist or is not a file, this returns false.  The time is returned as a
// TimePoint.
TimePoint path_get_last_modified_time(string path);

// Combine a valid path with a directory or relative path.
string path_join(string path, string relative_path, Arena* arena);

// Get the sys-rooted file name.
string path_sys_filename(string path, Arena* arena);

//------------------------------------------------------------------------------
// File I/O
//------------------------------------------------------------------------------

// Initialise the file system.  This sets up the module state only; it does not
// register `home`, `data`, `temp`, `cfg`, or `app`.
void file_init(void);

// Shutdown the file system.
void file_done(void);

// Path roots initialisation.  Call the helpers for the roots you want to make
// available as named agnostic paths.
void file_add_home_root(void);
void file_add_data_root(void);
void file_add_temp_root(void);
void file_add_cfg_root(void);
void file_add_app_root(void);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
