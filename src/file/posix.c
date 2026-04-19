//------------------------------------------------------------------------------
// POSIX-only implementations of functions
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> def: _POSIX_C_SOURCE=200809L
//> def: _GNU_SOURCE

#include <file/internal.h>

#if OS_POSIX

#    include <dirent.h>
#    include <errno.h>
#    include <limits.h>
#    include <stdio.h>
#    include <sys/stat.h>
#    include <unistd.h>

//------------------------------------------------------------------------------

internal bool _path_remove_platform_path(string platform_path)
{
    struct stat st = {0};
    if (lstat((char*)platform_path.data, &st) != 0) {
        return errno == ENOENT;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir((char*)platform_path.data);
        if (dir == NULL) {
            return false;
        }

        bool success = true;
        for (;;) {
            struct dirent* entry = readdir(dir);
            if (entry == NULL) {
                break;
            }

            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            usize child_len = platform_path.count + 1 + strlen(entry->d_name);
            char* child_buf = ARRAY_ALLOC(char, child_len + 1);
            snprintf(child_buf,
                     child_len + 1,
                     "%.*s/%s",
                     STRINGV(platform_path),
                     entry->d_name);
            string child = string_from((u8*)child_buf, child_len);
            if (!_path_remove_platform_path(child)) {
                FREE(child_buf);
                success = false;
                break;
            }
            FREE(child_buf);
        }

        closedir(dir);
        if (!success) {
            return false;
        }

        return rmdir((char*)platform_path.data) == 0;
    }

    return unlink((char*)platform_path.data) == 0;
}

//------------------------------------------------------------------------------
// path_from_platform
//------------------------------------------------------------------------------

string path_from_platform(string platform_path, Arena* arena)
{
    _file_ensure_initialised();

    u8* path_start = ARENA_ALLOC_ARRAY(arena, u8, 0);
    memcpy(ARENA_ALLOC_ARRAY(arena, u8, 4), "sys:", 4);

    u8* scan = platform_path.data;
    u8* end  = platform_path.data + platform_path.count;

    if (scan == end) {
        return (string){.data = path_start, .count = 0};
    }

    if (*scan != '/') {
        usize path_capacity = 256;
        while (true) {
            char* cwd    = ARRAY_ALLOC(char, path_capacity);
            char* result = getcwd(cwd, path_capacity);
            if (result == NULL) {
                if (errno == ERANGE) {
                    FREE(cwd);
                    path_capacity *= 2;
                    continue;
                }
                FREE(cwd);
                return (string){0};
            }

            usize cwd_len = strlen(cwd);
            memcpy(ARENA_ALLOC_ARRAY(arena, char, cwd_len), cwd, cwd_len);
            memcpy(ARENA_ALLOC_ARRAY(arena, char, 1), "/", 1);
            FREE(cwd);
            break;
        }
    }

    memcpy(ARENA_ALLOC_ARRAY(arena, u8, (usize)(end - scan)),
           scan,
           (usize)(end - scan));

    u8* path_end                     = ARENA_ALLOC_ARRAY(arena, u8, 0);
    *ARENA_ALLOC_ARRAY(arena, u8, 1) = 0;
    string sys_path                  = {.data  = path_start,
                                        .count = (usize)(path_end - path_start)};

    _path_simplify(&sys_path);
    return sys_path;
}

//------------------------------------------------------------------------------
// _path_add_host_root
//------------------------------------------------------------------------------

bool _path_add_host_root(string name, string platform_path)
{
    _file_ensure_initialised();

    string converted_path =
        path_from_platform(platform_path, &g_file_system.arena);
    if (converted_path.count == 0) {
        return false;
    }

    if (!_path_register_root(name, converted_path, true)) {
        isize existing_index = -1;
        for (usize i = 0; i < array_count(g_file_system.roots); ++i) {
            if (string_equals(g_file_system.roots[i].name, name)) {
                existing_index = (isize)i;
                break;
            }
        }

        return existing_index >= 0 &&
               string_equals(g_file_system.roots[existing_index].path,
                             converted_path);
    }

    return true;
}

bool _path_add_host_root_if_exists(string name, string platform_path)
{
    _file_ensure_initialised();

    string converted_path =
        path_from_platform(platform_path, &g_file_system.arena);
    if (converted_path.count == 0) {
        return false;
    }

    if (!_path_register_root(name, converted_path, false)) {
        isize existing_index = -1;
        for (usize i = 0; i < array_count(g_file_system.roots); ++i) {
            if (string_equals(g_file_system.roots[i].name, name)) {
                existing_index = (isize)i;
                break;
            }
        }

        return existing_index >= 0 &&
               string_equals(g_file_system.roots[existing_index].path,
                             converted_path);
    }

    return true;
}

//------------------------------------------------------------------------------
// path_to_platform
//------------------------------------------------------------------------------

string path_to_platform(string path, Arena* arena)
{
    _file_ensure_initialised();

    string sys_path = path_sys_filename(path, arena);
    if (sys_path.count == 0) {
        return (string){0};
    }

    string root = path_get_root(sys_path);
    if (!string_equals(root, S("sys"))) {
        return (string){0};
    }

    string tail = {0};
    if (!string_split_once(sys_path, ":/", NULL, &tail)) {
        return (string){0};
    }

    if (tail.count == 0) {
        return S("/");
    }

    return string_format(arena, "/%.*s", STRINGV(tail));
}

//------------------------------------------------------------------------------
// path_exists
//------------------------------------------------------------------------------

bool path_exists(string path)
{
    _file_ensure_initialised();

    string platform_path = path_to_platform(path, temp_arena());
    if (platform_path.count == 0) {
        temp_arena_reset();
        return false;
    }

    struct stat st = {0};
    bool        ok = lstat((char*)platform_path.data, &st) == 0;
    temp_arena_reset();
    return ok;
}

//------------------------------------------------------------------------------
// path_is_directory
//------------------------------------------------------------------------------

bool path_is_directory(string path)
{
    _file_ensure_initialised();

    string platform_path = path_to_platform(path, temp_arena());
    if (platform_path.count == 0) {
        temp_arena_reset();
        return false;
    }

    struct stat st = {0};
    bool ok = stat((char*)platform_path.data, &st) == 0 && S_ISDIR(st.st_mode);
    temp_arena_reset();
    return ok;
}

//------------------------------------------------------------------------------
// path_is_file
//------------------------------------------------------------------------------

bool path_is_file(string path)
{
    _file_ensure_initialised();

    string platform_path = path_to_platform(path, temp_arena());
    if (platform_path.count == 0) {
        temp_arena_reset();
        return false;
    }

    struct stat st = {0};
    bool ok = stat((char*)platform_path.data, &st) == 0 && S_ISREG(st.st_mode);
    temp_arena_reset();
    return ok;
}

//------------------------------------------------------------------------------
// path_create_directory
//------------------------------------------------------------------------------

bool path_create_directory(string path)
{
    _file_ensure_initialised();

    if (!path_is_valid(path)) {
        return false;
    }

    if (path_exists(path)) {
        return path_is_directory(path);
    }

    string parent = path_get_parent(path);
    if (!string_equals(parent, path) && !path_exists(parent)) {
        if (!path_create_directory(parent)) {
            return false;
        }
    }

    string platform_path = path_to_platform(path, temp_arena());
    bool   ok = mkdir((char*)platform_path.data, 0777) == 0 || errno == EEXIST;
    temp_arena_reset();
    return ok;
}

//------------------------------------------------------------------------------
// path_delete
//------------------------------------------------------------------------------

bool path_delete(string path)
{
    _file_ensure_initialised();

    if (!path_exists(path)) {
        return true;
    }

    string platform_path = path_to_platform(path, temp_arena());
    bool   ok            = _path_remove_platform_path(platform_path);
    temp_arena_reset();
    return ok;
}

//------------------------------------------------------------------------------
// path_get_file_size
//------------------------------------------------------------------------------

bool path_get_file_size(string path, u64* size)
{
    _file_ensure_initialised();

    if (size == NULL) {
        return false;
    }

    string platform_path = path_to_platform(path, temp_arena());
    if (platform_path.count == 0) {
        temp_arena_reset();
        return false;
    }

    struct stat st = {0};
    bool ok = stat((char*)platform_path.data, &st) == 0 && S_ISREG(st.st_mode);
    if (ok) {
        *size = (u64)st.st_size;
    }

    temp_arena_reset();
    return ok;
}

//------------------------------------------------------------------------------
// path_get_last_modified_time
//------------------------------------------------------------------------------

TimePoint path_get_last_modified_time(string path)
{
    _file_ensure_initialised();

    string platform_path = path_to_platform(path, temp_arena());
    if (platform_path.count == 0) {
        temp_arena_reset();
        return 0;
    }

    struct stat st = {0};
    if (stat((char*)platform_path.data, &st) != 0 || !S_ISREG(st.st_mode)) {
        temp_arena_reset();
        return 0;
    }

    temp_arena_reset();
    return (TimePoint)time_from_secs((u64)st.st_mtime);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#endif // OS_POSIX
