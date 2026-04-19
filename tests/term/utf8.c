//> use: core term

#include <term/internal.h>
#include <test.h>

TEST_CASE(term_utf8, next_decodes_wide_characters)
{
    cstr  cursor = "\xE4\xB8\xADZ";
    u32   ch     = 0;
    usize bytes  = 0;
    usize width  = 0;

    term_utf8_next(&cursor, &ch, &bytes, &width);

    TEST_ASSERT_EQ(ch, 0x4E2D);
    TEST_ASSERT_EQ(bytes, 3);
    TEST_ASSERT_EQ(width, 2);
    TEST_ASSERT_EQ(*cursor, 'Z');
}

TEST_CASE(term_utf8, next_replaces_invalid_sequences)
{
    const char invalid_bytes[] = {(char)0xC0, 'X', '\0'};
    cstr       cursor          = invalid_bytes;
    u32        ch              = 0;
    usize      bytes           = 0;
    usize      width           = 0;

    term_utf8_next(&cursor, &ch, &bytes, &width);

    TEST_ASSERT_EQ(ch, ' ');
    TEST_ASSERT_EQ(bytes, 1);
    TEST_ASSERT_EQ(width, 1);
    TEST_ASSERT_EQ(*cursor, 'X');
}
