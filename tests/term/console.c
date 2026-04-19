//> use: core term

#include <term/internal.h>
#include <test.h>

#if OS_POSIX
internal void _term_test_console_reset_term_state(void)
{
    array_free(g_term.event_queue);
    g_term.event_queue   = NULL;
    g_term.key_modifiers = 0;
    _term_test_posix_clear_input_buffers();
}

internal void _term_test_console_feed_raw_events(TermConsole* console, usize max_steps)
{
    for (usize i = 0; i < max_steps; i++) {
        _term_test_posix_raw_key_once();

        for (;;) {
            TermEvent event = term_poll_event();
            if (event.kind == TERM_EVENT_NONE) {
                break;
            }
            term_console_send_event(console, event);
        }
    }
}
#endif

TEST_CASE(console, write_wrap_reflows_across_rows)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 5, 3));
    term_console_init(&console, &window, 16);
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
    term_console_init(&console, &window, 16);
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
    term_console_init(&console, &window, 16);
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
    term_console_init(&console, &window, 16);
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
    term_console_init(&console, &window, 16);
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
    term_console_init(&console, &window, 16);
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

TEST_CASE(console, enter_snaps_view_back_to_bottom)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 6, 2));
    term_console_init(&console, &window, 16);
    term_console_set_prompt(&console, S("> "));
    term_console_write_cstr(&console, "one");
    term_console_write_cstr(&console, "two");
    term_console_write_cstr(&console, "three");
    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_MOUSE, .mouse.wheel = 1});

    TEST_ASSERT_EQ(console.scroll_offset, 1);
    TEST_ASSERT(!console.auto_scroll);

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key = '\r'});

    TEST_ASSERT_EQ(console.scroll_offset, 0);
    TEST_ASSERT(console.auto_scroll);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, offset_window_renders_prompt_and_input)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(3, 2, 8, 2));
    term_console_init(&console, &window, 16);
    term_console_set_prompt(&console, S("> "));
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'x'});

    TEST_ASSERT_EQ(window.cells[0].ch, '>');
    TEST_ASSERT_EQ(window.cells[1].ch, ' ');
    TEST_ASSERT_EQ(window.cells[2].ch, 'x');

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, ansi_output_colours_survive_enter_redraw)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 40, 4));
    term_console_init(&console, &window, 16);
    term_console_write_cstr(
        &console,
        "\033[38;5;81m[tick 1]\033[0m \033[38;5;220m◆\033[0m "
        "\033[38;2;140;255;180m1000 ms elapsed\033[0m");

    u32 before_bracket_ink = window.cells[0].ink;
    u32 before_diamond_ink = window.cells[9].ink;
    u32 before_elapsed_ink = window.cells[11].ink;

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = '\r'});

    TEST_ASSERT_EQ(window.cells[0].ink, before_bracket_ink);
    TEST_ASSERT_EQ(window.cells[9].ink, before_diamond_ink);
    TEST_ASSERT_EQ(window.cells[11].ink, before_elapsed_ink);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, ansi_output_colours_survive_scroll_snap_redraw)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 24, 3));
    term_console_init(&console, &window, 32);
    term_console_write_cstr(
        &console,
        "\033[38;5;81m[tick 1]\033[0m \033[38;5;220m◆\033[0m "
        "\033[38;2;140;255;180m1000 ms elapsed\033[0m");
    term_console_write_cstr(
        &console,
        "\033[38;5;81m[tick 2]\033[0m \033[38;5;220m◆\033[0m "
        "\033[38;2;140;255;180m2000 ms elapsed\033[0m");
    term_console_write_cstr(
        &console,
        "\033[38;5;81m[tick 3]\033[0m \033[38;5;220m◆\033[0m "
        "\033[38;2;140;255;180m3000 ms elapsed\033[0m");

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_MOUSE, .mouse.wheel = 1});
    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key = '\r'});

    TEST_ASSERT_EQ(window.cells[0].ink, term_rgb(95, 215, 255));
    TEST_ASSERT_EQ(window.cells[9].ink, term_rgb(255, 215, 0));

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, truecolour_output_colours_survive_echo_append)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 40, 6));
    term_console_init(&console, &window, 32);
    term_console_write_cstr(
        &console,
        "\033[38;5;81m[tick 1]\033[0m \033[38;5;220m◆\033[0m "
        "\033[38;2;140;255;180m1000 ms elapsed\033[0m");
    term_console_write_cstr(
        &console,
        "\033[38;5;81m[tick 2]\033[0m \033[38;5;220m◆\033[0m "
        "\033[38;2;140;255;180m2000 ms elapsed\033[0m");
    term_console_write_cstr(
        &console,
        "\033[38;5;111mecho\033[0m \033[38;5;246m>\033[0m hello");

    TEST_ASSERT_EQ(window.cells[11].ink, term_rgb(140, 255, 180));
    TEST_ASSERT_EQ(window.cells[40 + 11].ink, term_rgb(140, 255, 180));

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, left_right_allows_inserting_inside_input)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 12, 2));
    term_console_init(&console, &window, 16);

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'a'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'c'});
    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_LEFT});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'b'});

    TEST_ASSERT_EQ(array_count(console.input), 3);
    TEST_ASSERT_EQ(console.input[0], 'a');
    TEST_ASSERT_EQ(console.input[1], 'b');
    TEST_ASSERT_EQ(console.input[2], 'c');
    TEST_ASSERT_EQ(console.input_cursor, 2);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, ctrl_word_navigation_and_backspace_work)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 24, 2));
    term_console_init(&console, &window, 16);
    term_console_write_cstr(&console, "out");

    cstr text = "one two three";
    for (usize i = 0; text[i] != 0; i++) {
        term_console_send_event(
            &console, (TermEvent){.kind = TERM_EVENT_KEY, .key = text[i]});
    }

    term_console_send_event(&console,
                            (TermEvent){.kind = TERM_EVENT_KEY,
                                        .key_code = TERM_KEY_LEFT,
                                        .key_modifiers = TERM_KEYMOD_CTRL});
    TEST_ASSERT_EQ(console.input_cursor, 8);

    term_console_send_event(&console,
                            (TermEvent){.kind = TERM_EVENT_KEY,
                                        .key_code = TERM_KEY_BACKSPACE,
                                        .key_modifiers = TERM_KEYMOD_CTRL});

    TEST_ASSERT_EQ(array_count(console.input), 9);
    TEST_ASSERT_EQ(console.input_cursor, 4);
    TEST_ASSERT_EQ(console.input[0], 'o');
    TEST_ASSERT_EQ(console.input[1], 'n');
    TEST_ASSERT_EQ(console.input[2], 'e');
    TEST_ASSERT_EQ(console.input[3], ' ');
    TEST_ASSERT_EQ(console.input[4], 't');

    term_console_send_event(&console,
                            (TermEvent){.kind = TERM_EVENT_KEY,
                                        .key_code = TERM_KEY_RIGHT,
                                        .key_modifiers = TERM_KEYMOD_CTRL});
    TEST_ASSERT_EQ(console.input_cursor, 9);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, home_end_and_history_navigation_work)
{
    TermWindow  window  = {0};
    TermConsole console = {0};
    string      input   = {0};

    term_window_init(&window, term_rect(0, 0, 16, 3));
    term_console_init(&console, &window, 16);

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'f'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'i'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'r'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 's'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 't'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = '\r'});
    TEST_ASSERT(term_console_get_input(&console, &input));

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 's'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'e'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'c'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'o'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'n'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'd'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = '\r'});
    TEST_ASSERT(term_console_get_input(&console, &input));

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_UP});
    TEST_ASSERT_EQ(array_count(console.input), 6);
    TEST_ASSERT_EQ(console.input[0], 's');

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_UP});
    TEST_ASSERT_EQ(array_count(console.input), 5);
    TEST_ASSERT_EQ(console.input[0], 'f');

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_DOWN});
    TEST_ASSERT_EQ(array_count(console.input), 6);
    TEST_ASSERT_EQ(console.input[0], 's');

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_HOME});
    TEST_ASSERT_EQ(console.input_cursor, 0);

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_END});
    TEST_ASSERT_EQ(console.input_cursor, 6);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, ctrl_home_end_control_history_scroll)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 8, 2));
    term_console_init(&console, &window, 16);
    term_console_enable_input(&console, false);
    term_console_write_cstr(&console, "one");
    term_console_write_cstr(&console, "two");
    term_console_write_cstr(&console, "three");

    term_console_send_event(&console,
                            (TermEvent){.kind = TERM_EVENT_KEY,
                                        .key_code = TERM_KEY_HOME,
                                        .key_modifiers = TERM_KEYMOD_CTRL});
    TEST_ASSERT_EQ(console.scroll_offset, 1);
    TEST_ASSERT(!console.auto_scroll);

    term_console_send_event(&console,
                            (TermEvent){.kind = TERM_EVENT_KEY,
                                        .key_code = TERM_KEY_END,
                                        .key_modifiers = TERM_KEYMOD_CTRL});
    TEST_ASSERT_EQ(console.scroll_offset, 0);
    TEST_ASSERT(console.auto_scroll);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, ctrl_l_clears_output)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 12, 2));
    term_console_init(&console, &window, 16);
    term_console_write_cstr(&console, "hello");

    term_console_send_event(&console,
                            (TermEvent){.kind = TERM_EVENT_KEY,
                                        .key = 12,
                                        .key_modifiers = TERM_KEYMOD_CTRL});

    TEST_ASSERT_EQ(array_count(console.history), 0);
    TEST_ASSERT(console.auto_scroll);
    TEST_ASSERT_EQ(console.scroll_offset, 0);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, delete_key_removes_character_under_cursor)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 12, 2));
    term_console_init(&console, &window, 16);

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'a'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'b'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'c'});
    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_LEFT});
    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_LEFT});
    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_DELETE});

    TEST_ASSERT_EQ(array_count(console.input), 2);
    TEST_ASSERT_EQ(console.input[0], 'a');
    TEST_ASSERT_EQ(console.input[1], 'c');
    TEST_ASSERT_EQ(console.input_cursor, 1);

    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, history_down_restores_saved_draft)
{
    TermWindow  window  = {0};
    TermConsole console = {0};
    string      input   = {0};

    term_window_init(&window, term_rect(0, 0, 16, 3));
    term_console_init(&console, &window, 16);

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'f'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'i'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'r'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 's'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 't'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = '\r'});
    TEST_ASSERT(term_console_get_input(&console, &input));

    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'd'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'r'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'a'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 'f'});
    term_console_send_event(&console, (TermEvent){.kind = TERM_EVENT_KEY, .key = 't'});

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_UP});
    TEST_ASSERT_EQ(array_count(console.input), 5);
    TEST_ASSERT_EQ(console.input[0], 'f');

    term_console_send_event(
        &console, (TermEvent){.kind = TERM_EVENT_KEY, .key_code = TERM_KEY_DOWN});
    TEST_ASSERT_EQ(array_count(console.input), 5);
    TEST_ASSERT_EQ(console.input[0], 'd');
    TEST_ASSERT_EQ(console.input[1], 'r');
    TEST_ASSERT_EQ(console.input[2], 'a');
    TEST_ASSERT_EQ(console.input[3], 'f');
    TEST_ASSERT_EQ(console.input[4], 't');
    TEST_ASSERT_EQ(console.input_cursor, 5);

    term_console_done(&console);
    term_window_done(&window);
}

#if OS_POSIX
TEST_CASE(console, posix_raw_ctrl_left_then_text_keeps_console_editable)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 24, 2));
    term_console_init(&console, &window, 16);
    _term_test_console_reset_term_state();

    cstr initial = "hello world";
    for (usize i = 0; initial[i] != 0; i++) {
        term_console_send_event(
            &console, (TermEvent){.kind = TERM_EVENT_KEY, .key = initial[i]});
    }

    _term_test_posix_set_read_bytes("\033[1;5Dxyz", 9);
    _term_test_console_feed_raw_events(&console, 8);

    TEST_ASSERT_EQ(array_count(console.input), 14);
    TEST_ASSERT_EQ(console.input_cursor, 9);
    TEST_ASSERT_EQ(console.input[0], 'h');
    TEST_ASSERT_EQ(console.input[6], 'x');
    TEST_ASSERT_EQ(console.input[7], 'y');
    TEST_ASSERT_EQ(console.input[8], 'z');
    TEST_ASSERT_EQ(console.input[9], 'w');

    _term_test_console_reset_term_state();
    term_console_done(&console);
    term_window_done(&window);
}

TEST_CASE(console, posix_raw_multiple_modified_arrows_then_text_work_end_to_end)
{
    TermWindow  window  = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, 24, 2));
    term_console_init(&console, &window, 16);
    _term_test_console_reset_term_state();

    cstr initial = "abc def";
    for (usize i = 0; initial[i] != 0; i++) {
        term_console_send_event(
            &console, (TermEvent){.kind = TERM_EVENT_KEY, .key = initial[i]});
    }

    _term_test_posix_set_read_bytes("\033[1;5D\033[1;5DZ", 13);
    _term_test_console_feed_raw_events(&console, 10);

    TEST_ASSERT_EQ(array_count(console.input), 8);
    TEST_ASSERT_EQ(console.input_cursor, 1);
    TEST_ASSERT_EQ(console.input[0], 'Z');
    TEST_ASSERT_EQ(console.input[1], 'a');
    TEST_ASSERT_EQ(console.input[2], 'b');
    TEST_ASSERT_EQ(console.input[3], 'c');

    _term_test_console_reset_term_state();
    term_console_done(&console);
    term_window_done(&window);
}
#endif
