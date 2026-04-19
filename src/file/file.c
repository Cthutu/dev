//------------------------------------------------------------------------------
// File system implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> def: _POSIX_C_SOURCE=200809L
//> def: _GNU_SOURCE

#include <file/internal.h>

#if OS_POSIX
#    include <sys/stat.h>
#endif

//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------

FileSystem g_file_system;
bool       g_file_system_initialised;

//------------------------------------------------------------------------------

void _file_ensure_initialised(void)
{
    if (!g_file_system_initialised) {
        file_init();
    }
}

internal string _file_copy_cstr(cstr value, Arena* arena)
{
    return string_format(arena, "%s", value ? value : "");
}

internal string _file_basename(string path)
{
    if (path.count == 0) {
        return path;
    }

    usize end = path.count;
    while (end > 0 && path.data[end - 1] == '/') {
        end--;
    }

    usize start = end;
    while (start > 0 && path.data[start - 1] != '/') {
        start--;
    }

    return (string){.data = path.data + start, .count = end - start};
}

internal string _file_dirname(string path)
{
    if (path.count == 0) {
        return path;
    }

    usize end = path.count;
    while (end > 1 && path.data[end - 1] == '/') {
        end--;
    }

    if (end == 1 && path.data[0] == '/') {
        return S("/");
    }

    usize slash = end;
    while (slash > 0 && path.data[slash - 1] != '/') {
        slash--;
    }

    if (slash == 0) {
        return (string){0};
    }

    if (slash == 1) {
        return S("/");
    }

    return (string){.data = path.data, .count = slash - 1};
}

internal bool _file_string_ends_with(string value, string suffix)
{
    if (value.count < suffix.count) {
        return false;
    }

    return memcmp(value.data + value.count - suffix.count, suffix.data, suffix.count) ==
           0;
}

internal string _file_copy_string(string value, Arena* arena)
{
    return string_format(arena, STRINGP, STRINGV(value));
}

internal string _file_join_platform_path(Arena* arena, string left, string right)
{
    if (left.count == 0) {
        return _file_copy_string(right, arena);
    }

    if (right.count == 0) {
        return _file_copy_string(left, arena);
    }

    if (left.data[left.count - 1] == '/') {
        return string_format(arena, STRINGP STRINGP, STRINGV(left), STRINGV(right));
    }

    return string_format(arena, STRINGP "/" STRINGP, STRINGV(left), STRINGV(right));
}

internal bool _file_platform_is_directory(string platform_path)
{
#if OS_POSIX
    if (platform_path.count == 0) {
        return false;
    }

    struct stat st = {0};
    return stat((char*)platform_path.data, &st) == 0 && S_ISDIR(st.st_mode);
#else
    UNUSED(platform_path);
    return false;
#endif
}

internal string _file_cwd(Arena* arena)
{
#if OS_POSIX
    usize capacity = 256;
    while (true) {
        char* buffer = ARRAY_ALLOC(char, capacity);
        if (getcwd(buffer, capacity) != NULL) {
            usize len  = strlen(buffer);
            char* copy = ARENA_ALLOC_ARRAY(arena, char, len + 1);
            memcpy(copy, buffer, len + 1);
            FREE(buffer);
            return string_from((u8*)copy, len);
        }

        FREE(buffer);
        capacity *= 2;
    }
#else
    UNUSED(arena);
    return (string){0};
#endif
}

internal string _file_executable_path(Arena* arena)
{
#if OS_POSIX
    usize capacity = 256;
    while (true) {
        char* buffer = ARRAY_ALLOC(char, capacity);
        isize len    = readlink("/proc/self/exe", buffer, capacity - 1);
        if (len < 0) {
            FREE(buffer);
            return _file_cwd(arena);
        }

        if ((usize)len >= capacity - 1) {
            FREE(buffer);
            capacity *= 2;
            continue;
        }

        buffer[len] = '\0';
        string result = string_format(arena, "%s", buffer);
        FREE(buffer);
        return result;
    }
#else
    UNUSED(arena);
    return (string){0};
#endif
}

internal string _file_executable_directory(Arena* arena)
{
    string exe_path = _file_executable_path(arena);
    string dir      = _file_dirname(exe_path);
    if (dir.count == 0) {
        return _file_cwd(arena);
    }

    return _file_copy_string(dir, arena);
}

internal string _file_executable_name(Arena* arena)
{
    string exe_path  = _file_executable_path(arena);
    string basename  = _file_basename(exe_path);
    string app_name  = _file_copy_string(basename, arena);
    string debug_tag = S("-debug");

    if (_file_string_ends_with(app_name, debug_tag)) {
        app_name.count -= debug_tag.count;
    }

    if (app_name.count == 0) {
        return S("app");
    }

    return app_name;
}

internal string _file_find_data_directory(Arena* arena)
{
    string app_dir  = _file_executable_directory(arena);
    string app_name = _file_executable_name(arena);

    string same_dir_data =
        _file_join_platform_path(arena, _file_join_platform_path(arena, app_dir, S("data")), app_name);
    if (_file_platform_is_directory(same_dir_data)) {
        return same_dir_data;
    }

    string current = app_dir;
    while (current.count > 0) {
        if (string_equals(_file_basename(current), S("_bin"))) {
            string parent = _file_dirname(current);
            if (parent.count == 0) {
                break;
            }

            string sibling_data = _file_join_platform_path(
                arena, _file_join_platform_path(arena, parent, S("data")), app_name);
            if (_file_platform_is_directory(sibling_data)) {
                return sibling_data;
            }
            break;
        }

        string parent = _file_dirname(current);
        if (parent.count == 0 || string_equals(parent, current)) {
            break;
        }
        current = parent;
    }

    return (string){0};
}

//------------------------------------------------------------------------------
// file_add_home_root
//------------------------------------------------------------------------------

void file_add_home_root(void)
{
#if OS_POSIX
    Arena* arena = temp_arena();
    string home  = _file_copy_cstr(getenv("HOME"), arena);
    if (home.count > 0) {
        _path_add_host_root(S("home"), home);
    }
    temp_arena_reset();
#endif
}

//------------------------------------------------------------------------------
// file_add_data_root
//------------------------------------------------------------------------------

void file_add_data_root(void)
{
    Arena* arena    = temp_arena();
    string data_path = S("home:/dev/data/demo-path");

    if (data_path.count > 0) {
        path_add_root(S("data"), data_path);
    }

    temp_arena_reset();
}

//------------------------------------------------------------------------------
// file_add_temp_root
//------------------------------------------------------------------------------

void file_add_temp_root(void)
{
#if OS_POSIX
    Arena* arena = temp_arena();
    string temp  = _file_copy_cstr(getenv("TMPDIR"), arena);
    if (temp.count == 0) {
        temp = S("/tmp");
    }
    _path_add_host_root(S("temp"), temp);
    temp_arena_reset();
#endif
}

//------------------------------------------------------------------------------
// file_add_cfg_root
//------------------------------------------------------------------------------

void file_add_cfg_root(void)
{
#if OS_POSIX
    Arena* arena        = temp_arena();
    string project_name = _file_executable_name(arena);
    string cfg_path = string_format(
        arena, "home:/.config/" STRINGP, STRINGV(project_name));
    path_add_root(S("cfg"), cfg_path);
    temp_arena_reset();
#endif
}

//------------------------------------------------------------------------------
// file_add_app_root
//------------------------------------------------------------------------------

void file_add_app_root(void)
{
    Arena* arena = temp_arena();
    string app   = _file_executable_directory(arena);
    if (app.count > 0) {
        _path_add_host_root(S("app"), app);
    }
    temp_arena_reset();
}

//------------------------------------------------------------------------------
// file_init
//------------------------------------------------------------------------------

void file_init(void)
{
    if (g_file_system_initialised) {
        return;
    }

    arena_init(&g_file_system.arena);
    g_file_system.roots = 0;
    array_more_capacity(g_file_system.roots, 16);

    temp_arena_init();

    g_file_system_initialised = true;
}

//------------------------------------------------------------------------------
// file_done
//------------------------------------------------------------------------------

void file_done(void)
{
    if (!g_file_system_initialised) {
        return;
    }

    array_done(g_file_system.roots);
    arena_done(&g_file_system.arena);

    g_file_system_initialised = false;
    temp_arena_done();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
