# How to compose UI with TermWindow

This guide covers the off-screen window API used for panels, boxes, and clipped subviews.

## Goal

Build UI into a `TermWindow`, then copy that window into the terminal framebuffer.

## Steps

1. Create a `TermWindow` with `term_window_init()`.
2. Fill it with `term_window_clear()` or `term_window_paint_rect()`.
3. Draw borders or shapes with `term_window_rect()` and `term_window_9slice()`.
4. Write text with `term_window_write_cstr()` or `term_window_format()`.
5. Draw the finished window with `term_window_draw()`.
6. Free its cell buffer with `term_window_done()` when you are finished with it.

## Example

```c
//> use: core term

#include <term/term.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    TermWindow panel = {0};

    term_init();

    while (term_loop()) {
        TermEvent event = term_poll_event();
        if (event.kind == TERM_EVENT_KEY && event.key == 'q') {
            term_done();
            continue;
        }

        TermSize size = term_size_get();
        if (size.width < 12 || size.height < 8) {
            term_fb_cls(COLOUR_WHITE, COLOUR_BLACK);
            term_fb_write_cstr(1, 1, "Terminal too small");
            term_fb_present();
            continue;
        }

        term_window_init(&panel, term_rect(2, 1, size.width - 4, size.height - 2));

        term_window_clear(&panel, ' ', term_rgb(236, 223, 204), term_rgb(20, 24, 32));
        term_window_9slice(&panel,
                           term_rect(0, 0, panel.rect.width, panel.rect.height),
                           "/-\\| |\\-/",
                           false);
        term_window_paint(&panel,
                          term_rect(0, 0, panel.rect.width, panel.rect.height),
                          term_rgb(255, 180, 90),
                          term_rgb(20, 24, 32));

        term_window_write_cstr(&panel, 2, 2, "Window composition");
        term_window_format(&panel, 2, 4, "Panel size: %ux%u",
                           panel.rect.width,
                           panel.rect.height);

        term_fb_cls(COLOUR_WHITE, COLOUR_BLACK);
        term_window_draw(&panel);
        term_fb_present();

        term_window_done(&panel);
    }

    return 0;
}
```

## Notes

- `TermWindow` writes are clipped to the window bounds.
- `term_window_draw()` also clips if the window extends past the framebuffer edges.
- `term_window_format()` uses the temp arena. If you call it in a long-running loop, reset that arena in your frame loop with `temp_arena_reset()`.
- Guard your layout calculations before subtracting from `u16` sizes.

## Related files

- [term.h](/home/matt/dev/src/term/term.h)
- [window.c](/home/matt/dev/src/term/window.c)
- [demo-window.c](/home/matt/dev/src/demo-window.c)
