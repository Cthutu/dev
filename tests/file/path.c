//> use: core file
//> def: _POSIX_C_SOURCE=200809L
//> def: _GNU_SOURCE

#include <file/file.h>
#include <test.h>

#if OS_POSIX
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <unistd.h>
#endif

static void test_init_arena(Arena* arena)
{
    arena_init(arena, .reserved_size = 4096, .grow_rate = 1);
}

#if OS_POSIX
static string test_make_temp_directory(Arena* arena)
{
    char  template[] = "/tmp/file-tests-XXXXXX";
    char* directory  = mkdtemp(template);

    TEST_ASSERT_NOT_NULL(directory);
    return string_format(arena, "%s", directory);
}

static void test_write_platform_file(const char* path,
                                     const char* mode,
                                     const char* text)
{
    FILE* file = fopen(path, mode);
    TEST_ASSERT_NOT_NULL(file);
    if (!file) {
        return;
    }

    size_t len = strlen(text);
    TEST_ASSERT_EQ(fwrite(text, 1, len, file), len);
    TEST_ASSERT_EQ(fclose(file), 0);
}
#endif

TEST_CASE(file, init_and_done_are_callable)
{
    file_init();
    file_done();

    file_init();
    file_done();

    TEST_ASSERT(1);
}

TEST_CASE(path, components_are_extracted_from_a_path)
{
    string path = S("home:/docs/archive.tar.gz");

    TEST_ASSERT(string_equals(path_get_root(path), S("home")));
    TEST_ASSERT(string_equals(path_get_parent(path), S("home:/docs")));
    TEST_ASSERT(string_equals(path_get_filename(path), S("archive.tar.gz")));
    TEST_ASSERT(string_equals(path_get_extension(path), S(".gz")));
    TEST_ASSERT(string_equals(path_get_stem(path), S("archive.tar")));

    TEST_ASSERT(string_equals(path_get_parent(S("home:/file.txt")), S("home:/")));
    TEST_ASSERT(string_equals(path_get_filename(S("home:/")), S("")));
}

TEST_CASE(path, validity_matches_the_documented_rules)
{
    TEST_ASSERT(path_is_valid(S("home:/")));
    TEST_ASSERT(path_is_valid(S("home:/docs/file.txt")));
    TEST_ASSERT(path_is_valid(S("sys:/tmp/file.txt")));

    TEST_ASSERT(!path_is_valid(S("file.txt")));
    TEST_ASSERT(!path_is_valid(S("home:file.txt")));
    TEST_ASSERT(!path_is_valid(S("home:/docs//file.txt")));
    TEST_ASSERT(!path_is_valid(S("home:/docs/./file.txt")));
    TEST_ASSERT(!path_is_valid(S("home:/docs/../file.txt")));
    TEST_ASSERT(!path_is_valid(S("home:/docs/")));
}

TEST_CASE(path, join_appends_relative_paths)
{
    Arena arena;
    test_init_arena(&arena);

    string joined = path_join(S("home:/docs"), S("letters/report.txt"), &arena);
    TEST_ASSERT(string_equals(joined, S("home:/docs/letters/report.txt")));

    arena_reset(&arena);
    joined = path_join(S("home:/"), S("letters/report.txt"), &arena);
    TEST_ASSERT(string_equals(joined, S("home:/letters/report.txt")));

    arena_done(&arena);
}

#if OS_POSIX
TEST_CASE(path, platform_conversion_and_custom_roots_round_trip)
{
    file_init();

    Arena arena;
    test_init_arena(&arena);
    Arena expected_arena;
    test_init_arena(&expected_arena);

    char cwd[1024] = {0};
    TEST_ASSERT_NOT_NULL(getcwd(cwd, sizeof(cwd)));

    string absolute = path_from_platform(S("/tmp/file.txt"), &arena);
    TEST_ASSERT(string_equals(absolute, S("temp:/file.txt")));

    arena_reset(&arena);
    string relative = path_from_platform(S("notes/todo.txt"), &arena);
    string expected_relative =
        string_format(&arena, "sys:%s/notes/todo.txt", cwd);
    TEST_ASSERT(string_equals(relative, expected_relative));

    arena_reset(&arena);
    string sandbox_dir = test_make_temp_directory(&arena);
    string sandbox_dir_copy =
        string_format(&expected_arena, STRINGP, STRINGV(sandbox_dir));
    string sandbox_sys = path_from_platform(sandbox_dir, &arena);
    path_add_root(S("sandbox"), sandbox_sys);

    string sandbox_file = S("sandbox:/nested/file.txt");

    arena_reset(&arena);
    string platform_path = path_to_platform(sandbox_file, &arena);
    string expected_platform =
        string_format(&expected_arena,
                      "%.*s/nested/file.txt",
                      STRINGV(sandbox_dir_copy));
    TEST_ASSERT(string_equals(platform_path, expected_platform));

    arena_reset(&arena);
    string round_trip = path_from_platform(expected_platform, &arena);
    TEST_ASSERT(string_equals(round_trip, sandbox_file));

    arena_reset(&arena);
    string sys_filename = path_sys_filename(sandbox_file, &arena);
    string expected_sys =
        string_format(&expected_arena,
                      "sys:%.*s/nested/file.txt",
                      STRINGV(sandbox_dir_copy));
    TEST_ASSERT(string_equals(sys_filename, expected_sys));

    rmdir((char*)sandbox_dir.data);
    arena_done(&arena);
    arena_done(&expected_arena);
    file_done();
}

TEST_CASE(path, file_system_queries_and_mutations_use_paths)
{
    file_init();

    Arena arena;
    test_init_arena(&arena);

    string sandbox_dir = test_make_temp_directory(&arena);
    string sandbox_sys = path_from_platform(sandbox_dir, &arena);
    path_add_root(S("sandboxfs"), sandbox_sys);

    string nested_dir = S("sandboxfs:/alpha/beta");
    string file_path  = S("sandboxfs:/alpha/beta/data.bin");

    TEST_ASSERT(!path_exists(nested_dir));
    TEST_ASSERT(path_create_directory(nested_dir));
    TEST_ASSERT(path_exists(nested_dir));
    TEST_ASSERT(path_is_directory(nested_dir));
    TEST_ASSERT(!path_is_file(nested_dir));

    arena_reset(&arena);
    string platform_file = path_to_platform(file_path, &arena);
    test_write_platform_file((char*)platform_file.data, "wb", "hello world");

    TEST_ASSERT(path_exists(file_path));
    TEST_ASSERT(path_is_file(file_path));
    TEST_ASSERT(!path_is_directory(file_path));

    u64 size = 0;
    TEST_ASSERT(path_get_file_size(file_path, &size));
    TEST_ASSERT_EQ(size, 11);

    TimePoint first_modified = path_get_last_modified_time(file_path);
    TEST_ASSERT_GT(first_modified, 0);

    time_sleep_ms(1100);
    test_write_platform_file((char*)platform_file.data, "ab", "!");

    TimePoint second_modified = path_get_last_modified_time(file_path);
    TEST_ASSERT_GE(second_modified, first_modified);

    TEST_ASSERT(path_delete(S("sandboxfs:/alpha")));
    TEST_ASSERT(!path_exists(nested_dir));
    TEST_ASSERT(!path_exists(file_path));

    rmdir((char*)sandbox_dir.data);
    arena_done(&arena);
    file_done();
}
#endif
