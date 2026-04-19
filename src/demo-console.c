//> use: core term
//> desc: Interactive TermConsole demo

#include <term/term.h>

typedef struct {
    TermRect frame_rect;
    TermRect console_rect;
} DemoConsoleRects;

internal DemoConsoleRects demo_console_rects(TermSize size)
{
    DemoConsoleRects rects  = {0};
    u16              margin = 2;

    if (size.width <= margin * 2 || size.height <= margin * 2) {
        rects.console_rect = term_rect(0, 0, size.width, size.height);
        return rects;
    }

    rects.frame_rect =
        term_rect(margin, margin, size.width - margin * 2, size.height - margin * 2);

    if (rects.frame_rect.width > 2 && rects.frame_rect.height > 2) {
        rects.console_rect = term_rect(rects.frame_rect.x + 1,
                                       rects.frame_rect.y + 1,
                                       rects.frame_rect.width - 2,
                                       rects.frame_rect.height - 2);
    } else {
        rects.console_rect = rects.frame_rect;
    }

    return rects;
}

internal void demo_console_draw_frame(TermWindow* window)
{
    TermSize          size  = term_size_get();
    DemoConsoleRects  rects = demo_console_rects(size);
    const u32         paper = term_rgb(14, 18, 28);
    const u32         edge  = term_rgb(255, 170, 70);
    const u32         glow  = term_rgb(110, 210, 255);

    term_fb_cls(term_rgb(210, 220, 240), paper);

    if (!term_rect_is_empty(rects.frame_rect)) {
        term_fb_rect_colour(rects.frame_rect, edge, term_rgb(28, 34, 52));
        term_fb_9slice(rects.frame_rect, "/-\\| |\\-/", false);

        if (rects.frame_rect.width > 4) {
            term_fb_rect_colour(
                term_rect(rects.frame_rect.x + 2, rects.frame_rect.y, 18, 1),
                glow,
                term_rgb(28, 34, 52));
            term_fb_write_cstr(
                rects.frame_rect.x + 2, rects.frame_rect.y, " CONSOLE FRAME ");
        }
    }

    term_window_draw(window);
}

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    term_init();

    TermSize          size      = term_size_get();
    DemoConsoleRects  rects     = demo_console_rects(size);
    TermWindow        window    = {0};
    TermConsole       console   = {0};
    TimePoint  start_time = time_now();
    TimePoint  next_log   = time_add_duration(start_time, time_from_secs(1));
    u32        tick     = 1;

    term_window_init(&window, rects.console_rect);
    term_console_init(&console, &window, 256);
    term_console_set_output_colour(&console, term_rgb(230, 236, 255));
    term_console_set_prompt_colour(&console, term_rgb(120, 220, 255));
    term_console_set_input_colour(&console, term_rgb(255, 225, 120));
    term_console_set_prompt(&console, S("> "));
    term_console_write_cstr(&console,
                            "TermConsole demo. Type text and press ENTER. "
                            "Type q to quit.");
    demo_console_draw_frame(&window);

    while (term_loop()) {
        temp_arena_reset();
        bool redraw = false;

        TermEvent event = term_poll_event();
        if (event.kind != TERM_EVENT_NONE) {
            if (event.kind == TERM_EVENT_RESIZE) {
                rects = demo_console_rects(event.size);
                term_console_resize(&console, rects.console_rect);
            } else {
                term_console_send_event(&console, event);
            }
            redraw = true;
        }

        TimePoint now = time_now();
        if (now >= next_log) {
            TimeDuration elapsed = time_elapsed(start_time, now);
            term_console_format(
                &console,
                "\033[38;5;81m[tick %u]\033[0m "
                "\033[38;5;220m◆\033[0m "
                "\033[38;5;114m%llu ms elapsed\033[0m",
                tick++,
                time_duration_to_ms(elapsed));
            next_log = time_add_duration(now, time_from_secs(1));
            redraw = true;
        }

        string input = {0};
        if (term_console_get_input(&console, &input)) {
            if (string_equals_cstr(input, "q")) {
                term_done();
            } else {
                term_console_format(
                    &console,
                    "\033[38;5;111mecho\033[0m \033[38;5;246m>\033[0m " STRINGP,
                    STRINGV(input));
            }
            term_console_resize(&console, console.window->rect);
            redraw = true;
        }

        if (redraw) {
            demo_console_draw_frame(&window);
        }
    }

    term_console_done(&console);
    term_window_done(&window);
    return 0;
}
