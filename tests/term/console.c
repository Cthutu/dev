//> use: core term

#include <term/term.h>
#include <test.h>

TEST_CASE(console, write_wrap_reflows_across_rows)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 5, 3));
    term_console_init(&console, &window, COLOUR_WHITE, COLOUR_BLACK, 16);
    term_console_enable_input(&console, false);

    term_console_write_wrap(&console, S("ABCDEF"));

    TEST_ASSERT_EQ(window.cells[0].ch, 'A');
    TEST_ASSERT_EQ(window.cells[1].ch, 'B');
    TEST_ASSERT_EQ(window.cells[2].ch, 'C');
    TEST_ASSERT_EQ(window.cells[3].ch, 'D');
    TEST_ASSERT_EQ(window.cells[4].ch, 'E');
    TEST_ASSERT_EQ(window.cells[5].ch, 'F');

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, accepted_input_is_returned_once_and_editor_clears)
{
    TermWindow  window  = {0};
    TermConsole console = {0};
    string      input   = {0};

    term_window_init(&window, term_rect(0, 0, 8, 2));
    term_console_init(&console, &window, COLOUR_WHITE, COLOUR_BLACK, 16);
    term_console_set_prompt(&console, S("> "));

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'h'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'i'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = '\r'});

    TEST_ASSERT(term_console_get_input(&console, &input));
    TEST_ASSERT_EQ(input.count, 2);
    TEST_ASSERT_EQ(input.data[0], 'h');
    TEST_ASSERT_EQ(input.data[1], 'i');
    TEST_ASSERT(!term_console_get_input(&console, &input));

    TEST_ASSERT_EQ(window.cells[0].ch, '>');
    TEST_ASSERT_EQ(window.cells[1].ch, ' ');
    TEST_ASSERT_EQ(window.cells[2].ch, ' ');

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, separate_writes_start_on_separate_lines)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 8, 3));
    term_console_init(&console, &window, COLOUR_WHITE, COLOUR_BLACK, 16);
    term_console_enable_input(&console, false);

    term_console_write_cstr(&console, "first");
    term_console_write_cstr(&console, "second");

    TEST_ASSERT_EQ(window.cells[0].ch, 'f');
    TEST_ASSERT_EQ(window.cells[1].ch, 'i');
    TEST_ASSERT_EQ(window.cells[8].ch, 's');
    TEST_ASSERT_EQ(window.cells[9].ch, 'e');

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, resize_reflows_existing_history_into_new_width)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 4, 3));
    term_console_init(&console, &window, COLOUR_WHITE, COLOUR_BLACK, 16);
    term_console_enable_input(&console, false);
    term_console_write_wrap(&console, S("ABCDE"));

    term_console_resize(&console, term_rect(0, 0, 6, 3));

    TEST_ASSERT_EQ(window.rect.width, 6);
    TEST_ASSERT_EQ(window.cells[0].ch, 'A');
    TEST_ASSERT_EQ(window.cells[1].ch, 'B');
    TEST_ASSERT_EQ(window.cells[2].ch, 'C');
    TEST_ASSERT_EQ(window.cells[3].ch, 'D');
    TEST_ASSERT_EQ(window.cells[4].ch, 'E');

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, mouse_wheel_scrolls_history_without_touching_input)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 6, 2));
    term_console_init(&console, &window, COLOUR_WHITE, COLOUR_BLACK, 16);
    term_console_enable_input(&console, false);
    term_console_write_cstr(&console, "one");
    term_console_write_cstr(&console, "two");
    term_console_write_cstr(&console, "three");

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_MOUSE, .mouse.wheel = 1});

    TEST_ASSERT_EQ(console.scroll_offset, 1);
    TEST_ASSERT_EQ(window.cells[0].ch, 'o');
    TEST_ASSERT_EQ(window.cells[6].ch, 't');

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, new_output_does_not_snap_when_user_has_scrolled_up)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 6, 2));
    term_console_init(&console, &window, COLOUR_WHITE, COLOUR_BLACK, 16);
    term_console_enable_input(&console, false);
    term_console_write_cstr(&console, "one");
    term_console_write_cstr(&console, "two");
    term_console_write_cstr(&console, "three");
    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_MOUSE, .mouse.wheel = 1});

    term_console_write_cstr(&console, "four");

    TEST_ASSERT_EQ(console.scroll_offset, 2);
    TEST_ASSERT(!console.auto_scroll);
    TEST_ASSERT_EQ(window.cells[0].ch, 'o');
    TEST_ASSERT_EQ(window.cells[6].ch, 't');

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_MOUSE, .mouse.wheel = -2});

    TEST_ASSERT_EQ(console.scroll_offset, 0);
    TEST_ASSERT(console.auto_scroll);

    term_console_done(&console);
    term_window_done(&window);
}
