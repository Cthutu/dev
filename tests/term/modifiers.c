//> use: core term

#include <term/internal.h>
#include <test.h>

TEST_CASE(term, key_modifier_helpers_reflect_last_polled_key_event)
{
    g_term.key_modifiers = 0;
    array_push(g_term.event_queue,
               ((TermEvent){.kind = TERM_EVENT_KEY,
                            .key = 'a',
                            .key_modifiers = TERM_KEYMOD_CTRL |
                                             TERM_KEYMOD_SHIFT}));

    TermEvent event = term_poll_event();

    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(term_key_modifiers(),
                   (u8)(TERM_KEYMOD_CTRL | TERM_KEYMOD_SHIFT));
    TEST_ASSERT(term_key_ctrl_pressed());
    TEST_ASSERT(term_key_shift_pressed());
    TEST_ASSERT(!term_key_alt_pressed());

    array_free(g_term.event_queue);
    g_term.key_modifiers = 0;
}

#if OS_POSIX
internal void _term_test_reset_state(void)
{
    array_free(g_term.event_queue);
    g_term.event_queue   = NULL;
    g_term.key_modifiers = 0;
    _term_test_posix_clear_input_buffers();
}

TEST_CASE(term, posix_escape_tail_bytes_become_normal_keys_after_ctrl_left)
{
    cstr burst = "\033[1;5Dxyz";

    _term_test_reset_state();
    _term_test_posix_set_escape_buffer(burst, 9);

    TEST_ASSERT(_term_test_posix_drain_escape_buffer_once());

    TermEvent event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_LEFT);
    TEST_ASSERT_EQ(event.key_modifiers, TERM_KEYMOD_CTRL);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_NONE);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'x');
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_NONE);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'y');
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_NONE);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'z');
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_NONE);

    _term_test_reset_state();
}

TEST_CASE(term, posix_raw_key_handles_ctrl_left_and_trailing_text_in_one_burst)
{
    cstr burst = "\033[1;5Dxyz";

    _term_test_reset_state();
    _term_test_posix_set_read_bytes(burst, 9);

    _term_test_posix_raw_key_once();
    TermEvent event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_LEFT);
    TEST_ASSERT_EQ(event.key_modifiers, TERM_KEYMOD_CTRL);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_NONE);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'x');

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'y');

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'z');

    _term_test_reset_state();
}

TEST_CASE(term, posix_raw_key_handles_split_ctrl_left_sequence_then_text)
{
    _term_test_reset_state();

    _term_test_posix_set_read_bytes("\033", 1);
    _term_test_posix_raw_key_once();
    TermEvent event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_NONE);

    _term_test_posix_set_read_bytes("[1;5Dxy", 7);
    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_LEFT);
    TEST_ASSERT_EQ(event.key_modifiers, TERM_KEYMOD_CTRL);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_NONE);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'x');

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'y');

    _term_test_reset_state();
}

TEST_CASE(term, posix_raw_key_handles_back_to_back_modified_arrows_and_text)
{
    cstr burst = "\033[1;5D\033[1;5Cab";

    _term_test_reset_state();
    _term_test_posix_set_read_bytes(burst, 14);

    _term_test_posix_raw_key_once();
    TermEvent event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_LEFT);
    TEST_ASSERT_EQ(event.key_modifiers, TERM_KEYMOD_CTRL);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key_code, TERM_KEY_RIGHT);
    TEST_ASSERT_EQ(event.key_modifiers, TERM_KEYMOD_CTRL);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_NONE);

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'a');

    _term_test_posix_raw_key_once();
    event = term_poll_event();
    TEST_ASSERT_EQ(event.kind, TERM_EVENT_KEY);
    TEST_ASSERT_EQ(event.key, 'b');

    _term_test_reset_state();
}
#endif
