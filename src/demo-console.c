//> use: core term
//> desc: Interactive TermConsole demo

#include <term/term.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    term_init();

    TermSize   size    = term_size_get();
    TermWindow window  = {0};
    TermConsole console = {0};
    TimePoint  start_time = time_now();
    TimePoint  next_log   = time_add_duration(start_time, time_from_secs(1));
    u32        tick     = 1;

    term_window_init(&window, term_rect(0, 0, size.width, size.height));
    term_console_init(&console, &window, COLOUR_WHITE, COLOUR_BLACK, 256);
    term_console_set_prompt(&console, S("> "));
    term_console_write_cstr(&console,
                            "TermConsole demo. Type text and press ENTER. "
                            "Type q to quit.");

    while (term_loop()) {
        temp_arena_reset();

        TermEvent event = term_poll_event();
        if (event.kind != TERM_EVENT_NONE) {
            term_console_send_event(&console, event);
        }

        TimePoint now = time_now();
        if (now >= next_log) {
            TimeDuration elapsed = time_elapsed(start_time, now);
            term_console_format(
                &console, "[tick %u] %llu ms elapsed", tick++, time_duration_to_ms(elapsed));
            next_log = time_add_duration(now, time_from_secs(1));
        }

        string input = {0};
        if (term_console_get_input(&console, &input)) {
            if (string_equals_cstr(input, "q")) {
                term_done();
            } else {
                term_console_format(
                    &console, "echo: " STRINGP, STRINGV(input));
            }
        }
    }

    term_console_done(&console);
    term_window_done(&window);
    return 0;
}
