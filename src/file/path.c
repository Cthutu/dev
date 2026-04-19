//------------------------------------------------------------------------------
// Path implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <file/internal.h>

#include <stdio.h>

//------------------------------------------------------------------------------

internal bool _path_split_root(string path, string* out_root, string* out_tail)
{
    string root = {0};
    string tail = {0};
    if (!string_split_once(path, ":/", &root, &tail)) {
        return false;
    }

    if (out_root) {
        *out_root = root;
    }
    if (out_tail) {
        *out_tail = tail;
    }
    return true;
}

internal isize _path_find_root_index(string name)
{
    for (usize i = 0; i < array_count(g_file_system.roots); ++i) {
        if (string_equals(name, g_file_system.roots[i].name)) {
            return (isize)i;
        }
    }

    return -1;
}

internal bool _path_root_exists(string name)
{
    return string_equals(name, S("sys")) || _path_find_root_index(name) >= 0;
}

internal bool _path_root_prefix_matches(string path, string prefix)
{
    if (path.count < prefix.count) {
        return false;
    }

    if (memcmp(path.data, prefix.data, prefix.count) != 0) {
        return false;
    }

    if (path.count == prefix.count) {
        return true;
    }

    return path.data[prefix.count] == '/';
}

internal string _path_expand_to_sys(string path, Arena* arena)
{
    string root = {0};
    string tail = {0};
    if (!_path_split_root(path, &root, &tail)) {
        return (string){0};
    }

    if (string_equals(root, S("sys"))) {
        return string_format(arena, STRINGP ":/" STRINGP, STRINGV(root), STRINGV(tail));
    }

    isize root_index = _path_find_root_index(root);
    if (root_index < 0) {
        return (string){0};
    }

    string root_path = g_file_system.roots[root_index].path;
    string sys_root_path = _path_expand_to_sys(root_path, arena);
    if (sys_root_path.count == 0) {
        return (string){0};
    }

    if (tail.count == 0) {
        return string_format(arena, STRINGP, STRINGV(sys_root_path));
    }

    if (sys_root_path.data[sys_root_path.count - 1] == '/') {
        return string_format(arena,
                             STRINGP STRINGP,
                             STRINGV(sys_root_path),
                             STRINGV(tail));
    }

    return string_format(
        arena, STRINGP "/" STRINGP, STRINGV(sys_root_path), STRINGV(tail));
}

//------------------------------------------------------------------------------
// path_char_is_valid
//------------------------------------------------------------------------------

bool path_char_is_valid(u32 codepoint)
{
    if (codepoint < 0x20 || codepoint == 0x7f) {
        return false;
    }

    switch (codepoint) {
    case '<':
    case '>':
    case '"':
    case '|':
    case '?':
    case '*':
    case '\\':
    case ':':
        return false;
    default:
        return true;
    }
}

//------------------------------------------------------------------------------
// path_is_valid
//------------------------------------------------------------------------------

bool path_is_valid(string path)
{
    _file_ensure_initialised();

    string root_name     = {0};
    string relative_path = {0};
    if (!_path_split_root(path, &root_name, &relative_path)) {
        return false;
    }

    if (!_path_root_exists(root_name)) {
        return false;
    }

    if (relative_path.count == 0) {
        return true;
    }

    bool at_component_start = true;
    for (usize i = 0; i < relative_path.count;) {
        u32   codepoint = 0;
        usize bytes     = string_utf8_decode(relative_path.data + i, &codepoint);
        if (bytes == 0) {
            return false;
        }

        if (!path_char_is_valid(codepoint)) {
            return false;
        }

        if (codepoint == '/') {
            if (at_component_start) {
                return false;
            }
            at_component_start = true;
            i += bytes;
            continue;
        }

        if (codepoint == '.') {
            usize component_len = 0;
            while (i + component_len < relative_path.count &&
                   relative_path.data[i + component_len] != '/') {
                component_len++;
            }

            if (component_len == 1 ||
                (component_len == 2 && relative_path.data[i + 1] == '.')) {
                return false;
            }
        }

        at_component_start = false;
        i += bytes;
    }

    if (relative_path.data[relative_path.count - 1] == '/') {
        return false;
    }

    return true;
}

internal bool _path_root_matches_existing(string name, string sys_path)
{
    isize existing_index = _path_find_root_index(name);
    if (existing_index < 0) {
        return false;
    }

    string existing_sys_path =
        path_sys_filename(g_file_system.roots[existing_index].path, temp_arena());
    if (existing_sys_path.count == 0) {
        temp_arena_reset();
        return false;
    }

    bool same = string_equals(existing_sys_path, sys_path);
    temp_arena_reset();
    return same;
}

bool _path_register_root(string name, string path, bool create_if_missing)
{
    _file_ensure_initialised();

    ASSERT(path.count >= name.count,
           "Root path must be equal or longer than root name");

    if (name.count == 0) {
        return false;
    }

    if (!path_is_valid(path)) {
        return false;
    }

    string root_name = path_get_root(path);
    if (string_equals(name, root_name)) {
        return false;
    }

    // Keep a stable copy of the agnostic path because nested helpers such as
    // path_exists/path_create_directory use and reset the temp arena.
    string stored_path =
        string_format(&g_file_system.arena, STRINGP, STRINGV(path));

    string sys_path = path_sys_filename(path, temp_arena());
    if (sys_path.count == 0) {
        temp_arena_reset();
        return false;
    }

    if (_path_find_root_index(name) >= 0) {
        bool same = _path_root_matches_existing(name, sys_path);
        temp_arena_reset();
        return same;
    }

    if (!path_exists(path)) {
        if (!create_if_missing || !path_create_directory(path)) {
            temp_arena_reset();
            return false;
        }
    } else if (!path_is_directory(path)) {
        temp_arena_reset();
        return false;
    }

    Arena* arena = &g_file_system.arena;
    array_push(
        g_file_system.roots,
        ((FileRoot){
            .name = string_format(arena, STRINGP, STRINGV(name)),
            .path = stored_path,
        }));

    temp_arena_reset();
    return true;
}

//------------------------------------------------------------------------------
// path_add_root
//------------------------------------------------------------------------------

bool path_add_root(string name, string path)
{
    return _path_register_root(name, path, true);
}

//------------------------------------------------------------------------------
// path_get_root
//------------------------------------------------------------------------------

string path_get_root(string path)
{
    string root = {0};
    if (string_split_once(path, ":/", &root, NULL)) {
        return root;
    }
    return (string){0};
}

//------------------------------------------------------------------------------
// path_get_parent
//------------------------------------------------------------------------------

string path_get_parent(string path)
{
    if (path.count == 0) {
        return (string){0};
    }

    if (path.count >= 2 && path.data[path.count - 1] == '/' &&
        path.data[path.count - 2] == ':') {
        return path;
    }

    for (usize i = path.count; i > 0; --i) {
        if (path.data[i - 1] != '/') {
            continue;
        }

        return (string){.data = path.data,
                        .count = (i >= 2 && path.data[i - 2] == ':') ? i : i - 1};
    }

    return path;
}

//------------------------------------------------------------------------------
// path_get_filename
//------------------------------------------------------------------------------

string path_get_filename(string path)
{
    if (path.count == 0) {
        return (string){0};
    }

    if (path.count >= 2 && path.data[path.count - 1] == '/' &&
        path.data[path.count - 2] == ':') {
        return (string){.data = path.data + path.count, .count = 0};
    }

    for (usize i = path.count; i > 0; --i) {
        if (path.data[i - 1] == '/') {
            return (string){.data = path.data + i, .count = path.count - i};
        }
    }

    return path;
}

//------------------------------------------------------------------------------
// path_get_extension
//------------------------------------------------------------------------------

string path_get_extension(string path)
{
    string filename = path_get_filename(path);
    for (usize i = filename.count; i > 0; --i) {
        if (filename.data[i - 1] == '.') {
            return (string){
                .data  = filename.data + i - 1,
                .count = filename.count - (i - 1),
            };
        }
    }

    return (string){.data = filename.data + filename.count, .count = 0};
}

//------------------------------------------------------------------------------
// path_get_stem
//------------------------------------------------------------------------------

string path_get_stem(string path)
{
    string filename  = path_get_filename(path);
    string extension = path_get_extension(path);
    if (extension.count == 0) {
        return filename;
    }

    return (string){.data = filename.data, .count = filename.count - extension.count};
}

//------------------------------------------------------------------------------
// path_join
//------------------------------------------------------------------------------

string path_join(string path, string relative_path, Arena* arena)
{
    if (relative_path.count == 0) {
        return string_format(arena, STRINGP, STRINGV(path));
    }

    if (path.count >= 2 && path.data[path.count - 1] == '/' &&
        path.data[path.count - 2] == ':') {
        return string_format(
            arena, STRINGP STRINGP, STRINGV(path), STRINGV(relative_path));
    }

    return string_format(
        arena, STRINGP "/" STRINGP, STRINGV(path), STRINGV(relative_path));
}

//------------------------------------------------------------------------------
// path_sys_filename
//------------------------------------------------------------------------------

string path_sys_filename(string path, Arena* arena)
{
    _file_ensure_initialised();
    return _path_expand_to_sys(path, arena);
}

//------------------------------------------------------------------------------
// _path_simplify
//------------------------------------------------------------------------------

internal void _path_simplify_internal(string* path, string skipped_root)
{
    if (path == NULL || path->count == 0) {
        return;
    }

    _file_ensure_initialised();

    isize best_index        = -1;
    usize best_prefix_count = 0;
    Arena* scratch          = temp_arena();

    for (usize i = 0; i < array_count(g_file_system.roots); ++i) {
        if (skipped_root.count > 0 &&
            string_equals(g_file_system.roots[i].name, skipped_root)) {
            continue;
        }

        string prefix = path_sys_filename(g_file_system.roots[i].path, scratch);
        if (prefix.count == 0) {
            continue;
        }

        if (!_path_root_prefix_matches(*path, prefix)) {
            continue;
        }

        if (prefix.count > best_prefix_count) {
            best_index        = (isize)i;
            best_prefix_count = prefix.count;
        }
    }

    if (best_index < 0) {
        temp_arena_reset();
        return;
    }

    FileRoot* root        = &g_file_system.roots[best_index];
    usize     tail_offset = best_prefix_count;
    if (tail_offset < path->count && path->data[tail_offset] == '/') {
        tail_offset++;
    }

    usize tail_count = path->count - tail_offset;
    char* tail = ARRAY_ALLOC(char, tail_count + 1);
    memcpy(tail, path->data + tail_offset, tail_count);
    tail[tail_count] = '\0';

    int written = snprintf((char*)path->data,
                           path->count + 1,
                           "%.*s:/%s",
                           STRINGV(root->name),
                           tail);

    if (written < 0) {
        FREE(tail);
        return;
    }

    if (tail_count == 0) {
        written = snprintf(
            (char*)path->data, path->count + 1, "%.*s:/", STRINGV(root->name));
    }

    FREE(tail);
    path->count = (usize)written;
    temp_arena_reset();
}

void _path_simplify(string* path)
{
    _path_simplify_internal(path, (string){0});
}

void _path_simplify_skipping_root(string* path, string skipped_root)
{
    _path_simplify_internal(path, skipped_root);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
