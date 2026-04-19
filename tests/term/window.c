//> use: core term

#include <term/internal.h>
#include <term/term.h>
#include <test.h>

TEST_CASE(window, ansi_reset_restores_painted_colours)
{
    TermWindow window = {0};
    term_window_init(&window, term_rect(0, 0, 8, 1));
    term_window_clear(&window, ' ', COLOUR_SOFT_WHITE, COLOUR_NIGHT_STONE);

    term_window_write_cstr(&window, 0, 0, "A\033[31mB\033[0mC");

    TEST_ASSERT_EQ(window.cells[0].ch, 'A');
    TEST_ASSERT_EQ(window.cells[0].ink, COLOUR_SOFT_WHITE);
    TEST_ASSERT_EQ(window.cells[0].paper, COLOUR_NIGHT_STONE);

    TEST_ASSERT_EQ(window.cells[1].ch, 'B');
    TEST_ASSERT_EQ(window.cells[1].ink, term_rgb(0x80, 0x00, 0x00));
    TEST_ASSERT_EQ(window.cells[1].paper, COLOUR_NIGHT_STONE);

    TEST_ASSERT_EQ(window.cells[2].ch, 'C');
    TEST_ASSERT_EQ(window.cells[2].ink, COLOUR_SOFT_WHITE);
    TEST_ASSERT_EQ(window.cells[2].paper, COLOUR_NIGHT_STONE);

    term_window_done(&window);
}

TEST_CASE(window, ansi_foreground_and_background_can_reset_independently)
{
    TermWindow window = {0};
    term_window_init(&window, term_rect(0, 0, 8, 1));
    term_window_clear(&window, ' ', COLOUR_SOFT_WHITE, COLOUR_NIGHT_STONE);

    term_window_write_cstr(
        &window, 0, 0, "\033[38;5;196;48;5;22mX\033[39mY\033[49mZ");

    TEST_ASSERT_EQ(window.cells[0].ink, term_rgb(0xFF, 0x00, 0x00));
    TEST_ASSERT_EQ(window.cells[0].paper, term_rgb(0, 95, 0));

    TEST_ASSERT_EQ(window.cells[1].ink, COLOUR_SOFT_WHITE);
    TEST_ASSERT_EQ(window.cells[1].paper, term_rgb(0, 95, 0));

    TEST_ASSERT_EQ(window.cells[2].ink, COLOUR_SOFT_WHITE);
    TEST_ASSERT_EQ(window.cells[2].paper, COLOUR_NIGHT_STONE);

    term_window_done(&window);
}

TEST_CASE(window, write_handles_wide_utf8_characters)
{
    TermWindow window = {0};
    term_window_init(&window, term_rect(0, 0, 6, 1));
    term_window_clear(&window, ' ', COLOUR_WHITE, COLOUR_BLACK);

    term_window_write_cstr(&window, 0, 0, "A中B");

    TEST_ASSERT_EQ(window.cells[0].ch, 'A');
    TEST_ASSERT_EQ(window.cells[1].ch, 0x4E2D);
    TEST_ASSERT_EQ(window.cells[2].ch, TERM_FB_CHAR_WIDE_TAIL);
    TEST_ASSERT_EQ(window.cells[3].ch, 'B');

    term_window_done(&window);
}
