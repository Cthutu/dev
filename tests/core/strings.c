//> use: core

#include <core/core.h>
#include <test.h>

TEST_CASE(string, from_cstr_and_from_bytes)
{
    string hello = string_from_cstr("hello");
    TEST_ASSERT_EQ(hello.count, 5);
    TEST_ASSERT_EQ(hello.data[0], 'h');

    u8     raw[] = {'a', 'b', 'c'};
    string bytes = string_from(raw, 3);
    TEST_ASSERT_EQ(bytes.count, 3);
    TEST_ASSERT_EQ(bytes.data[2], 'c');
}

TEST_CASE(string, format_uses_arena_storage)
{
    Arena arena;
    arena_init(&arena, .reserved_size = 4096, .grow_rate = 1);

    string text = string_format(&arena, "%s %d", "value", 42);
    TEST_ASSERT_EQ(text.count, 8);
    TEST_ASSERT_STR_EQ((char*)text.data, "value 42");

    arena_done(&arena);
}

TEST_CASE(string, builder_appends_and_formats)
{
    Arena arena;
    arena_init(&arena, .reserved_size = 4096, .grow_rate = 1);

    StringBuilder sb;
    sb_init(&sb, &arena);
    sb_append_cstr(&sb, "ab");
    sb_append_char(&sb, 'c');
    sb_append_string(&sb, string_from_cstr("de"));
    sb_format(&sb, "%d", 12);
    sb_append_null(&sb);

    string built = sb_to_string(&sb);
    TEST_ASSERT_EQ(built.count, 8);
    TEST_ASSERT_EQ(built.data[7], '\0');
    TEST_ASSERT_STR_EQ((char*)built.data, "abcde12");

    arena_done(&arena);
}

TEST_CASE(string, equals_and_equals_cstr_compare_contents)
{
    TEST_ASSERT(string_equals(S("hello"), S("hello")));
    TEST_ASSERT(!string_equals(S("hello"), S("world")));
    TEST_ASSERT(!string_equals(S("hello"), S("hell")));

    TEST_ASSERT(string_equals_cstr(S("hello"), "hello"));
    TEST_ASSERT(!string_equals_cstr(S("hello"), "hello!"));
}

TEST_CASE(string, split_once_splits_around_first_delimiter)
{
    string left  = {0};
    string right = {0};

    TEST_ASSERT(string_split_once(S("alpha::beta::gamma"), "::", &left, &right));
    TEST_ASSERT(string_equals(left, S("alpha")));
    TEST_ASSERT(string_equals(right, S("beta::gamma")));

    left  = (string){0};
    right = (string){0};
    TEST_ASSERT(!string_split_once(S("alpha"), "::", &left, &right));
    TEST_ASSERT_EQ(left.count, 0);
    TEST_ASSERT_EQ(right.count, 0);
}

TEST_CASE(string, split_returns_all_parts)
{
    Arena arena;
    arena_init(&arena, .reserved_size = 4096, .grow_rate = 1);

    strings parts = string_split(S("red,green,blue"), ",", &arena);

    TEST_ASSERT_EQ(parts.count, 3);
    TEST_ASSERT(string_equals(parts.data[0], S("red")));
    TEST_ASSERT(string_equals(parts.data[1], S("green")));
    TEST_ASSERT(string_equals(parts.data[2], S("blue")));

    arena_done(&arena);
}

TEST_CASE(string, to_u64_accepts_digits_and_rejects_other_input)
{
    u64 value = 0;

    TEST_ASSERT(string_to_u64(S("0"), &value));
    TEST_ASSERT_EQ(value, 0);

    TEST_ASSERT(string_to_u64(S("184467"), &value));
    TEST_ASSERT_EQ(value, 184467);

    TEST_ASSERT(string_to_u64(S(""), &value));
    TEST_ASSERT_EQ(value, 0);

    TEST_ASSERT(!string_to_u64(S("12x"), &value));
}

TEST_CASE(string, character_count_counts_visible_characters)
{
    TEST_ASSERT_EQ(string_character_count(S("")), 0);
    TEST_ASSERT_EQ(string_character_count(S("hello")), 5);
    TEST_ASSERT_EQ(string_character_count(S("\033[31mred\033[0m")), 3);
    TEST_ASSERT_EQ(string_character_count(S("a\033[1;32mb\033[0mc")), 3);
}

TEST_CASE(string, line_count_counts_newlines)
{
    TEST_ASSERT_EQ(string_line_count(S("")), 1);
    TEST_ASSERT_EQ(string_line_count(S("single line")), 1);
    TEST_ASSERT_EQ(string_line_count(S("one\ntwo")), 2);
    TEST_ASSERT_EQ(string_line_count(S("one\ntwo\n")), 3);
}
