//> use: core

#include <core/core.h>
#include <test.h>

TEST_CASE(unicode, char_width_handles_ascii_combining_and_wide)
{
    TEST_ASSERT_EQ(string_unicode_char_width('A'), 1);
    TEST_ASSERT_EQ(string_unicode_char_width(0), 0);
    TEST_ASSERT_EQ(string_unicode_char_width(0x0301), 0);
    TEST_ASSERT_EQ(string_unicode_char_width(0x4E2D), 2);
    TEST_ASSERT_EQ(string_unicode_char_width(0xFF21), 2);
}

TEST_CASE(unicode, utf8_decode_decodes_valid_sequences)
{
    u32 codepoint = 0;

    TEST_ASSERT_EQ(string_utf8_decode((const u8*)"A", &codepoint), 1);
    TEST_ASSERT_EQ(codepoint, 'A');

    TEST_ASSERT_EQ(string_utf8_decode((const u8*)"\xC3\xA9", &codepoint), 2);
    TEST_ASSERT_EQ(codepoint, 0x00E9);

    TEST_ASSERT_EQ(
        string_utf8_decode((const u8*)"\xE4\xB8\xAD", &codepoint), 3);
    TEST_ASSERT_EQ(codepoint, 0x4E2D);

    TEST_ASSERT_EQ(
        string_utf8_decode((const u8*)"\xF0\x9F\x98\x80", &codepoint), 4);
    TEST_ASSERT_EQ(codepoint, 0x1F600);
}

TEST_CASE(unicode, utf8_decode_rejects_invalid_sequences)
{
    u32 codepoint = 0;

    TEST_ASSERT_EQ(string_utf8_decode((const u8*)"\xC0\xAF", &codepoint), 1);
    TEST_ASSERT_EQ(codepoint, 0xFFFD);

    TEST_ASSERT_EQ(
        string_utf8_decode((const u8*)"\xED\xA0\x80", &codepoint), 1);
    TEST_ASSERT_EQ(codepoint, 0xFFFD);

    TEST_ASSERT_EQ(
        string_utf8_decode((const u8*)"\xF4\x90\x80\x80", &codepoint), 1);
    TEST_ASSERT_EQ(codepoint, 0xFFFD);
}

TEST_CASE(unicode, character_cell_count_ignores_ansi_and_combining_marks)
{
    TEST_ASSERT_EQ(string_character_cell_count(S("A\xCC\x81")), 1);
    TEST_ASSERT_EQ(
        string_character_cell_count(S("A" "\xE4\xB8\xAD" "B")), 4);
    TEST_ASSERT_EQ(
        string_character_cell_count(
            S("\033[31m" "A" "\xE4\xB8\xAD" "\xCC\x81" "B" "\033[0m")),
        4);
}

TEST_CASE(unicode, wrap_uses_display_width)
{
    Arena arena;
    arena_init(&arena, .reserved_size = 4096, .grow_rate = 1);

    strings wrapped = string_wrap(S("ab" "\xE4\xB8\xAD" "cd"), &arena, 4);

    TEST_ASSERT_EQ(wrapped.count, 2);
    TEST_ASSERT_EQ(wrapped.data[0].count, 5);
    TEST_ASSERT_MEM_EQ(wrapped.data[0].data, "ab\xE4\xB8\xAD", 5);
    TEST_ASSERT_EQ(wrapped.data[1].count, 2);
    TEST_ASSERT_MEM_EQ(wrapped.data[1].data, "cd", 2);

    arena_done(&arena);
}
