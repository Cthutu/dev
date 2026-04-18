# How to build layered panels like demo-window

This guide explains the composition pattern used by the animated window demo.

## Goal

Create several independent windows, render each one off-screen, then layer them
onto the framebuffer in draw order.

## Steps

1. Query the current terminal size.
2. Derive a rectangle for each panel from that size.
3. Initialise one `TermWindow` per panel.
4. Render each panel into its own cell buffer.
5. Clear the framebuffer.
6. Draw the windows in back-to-front order.
7. Present the final frame.

## Example

```c
//> use: core term

#include <term/term.h>

typedef struct {
    TermWindow backdrop;
    TermWindow status;
    TermWindow dialog;
} UiState;

internal void draw_box(TermWindow* window, cstr title, u32 ink, u32 paper)
{
    term_window_paint_rect(window,
                           term_rect(0, 0, window->rect.width, window->rect.height),
                           ' ',
                           ink,
                           paper);
    term_window_9slice(window,
                       term_rect(0, 0, window->rect.width, window->rect.height),
                       "/-\\| |\\-/",
                       false);
    term_window_write_cstr(window, 2, 0, title);
}

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    UiState ui = {0};

    term_init();

    while (term_loop()) {
        TermEvent event = term_poll_event();
        if (event.kind == TERM_EVENT_KEY && event.key == 'q') {
            term_done();
            continue;
        }

        TermSize size = term_size_get();
        if (size.width < 40 || size.height < 12) {
            term_fb_cls(COLOUR_WHITE, COLOUR_BLACK);
            term_fb_write_cstr(2, 2, "Terminal too small for layered panels");
            term_fb_present();
            continue;
        }

        term_window_init(&ui.backdrop, term_rect(0, 0, size.width, size.height));
        term_window_init(&ui.status, term_rect(0, size.height - 3, size.width, 3));
        term_window_init(&ui.dialog, term_rect(size.width / 2 - 16, size.height / 2 - 5, 32, 10));

        term_window_clear(&ui.backdrop, '.', term_rgb(90, 120, 140), term_rgb(10, 18, 28));
        draw_box(&ui.status, " STATUS ", term_rgb(255, 218, 130), term_rgb(42, 20, 8));
        draw_box(&ui.dialog, " DIALOG ", term_rgb(236, 223, 204), term_rgb(20, 24, 32));

        term_window_write_cstr(&ui.status, 2, 1, "Later windows overwrite earlier ones");
        term_window_write_cstr(&ui.dialog, 2, 2, "This panel sits on top of the backdrop");

        term_fb_cls(COLOUR_WHITE, COLOUR_BLACK);
        term_window_draw(&ui.backdrop);
        term_window_draw(&ui.status);
        term_window_draw(&ui.dialog);
        term_fb_present();

        term_window_done(&ui.backdrop);
        term_window_done(&ui.status);
        term_window_done(&ui.dialog);
        temp_arena_reset();
    }

    return 0;
}
```

## Notes

- Draw order is the layering rule. If two windows overlap, the one drawn later
  wins.
- The demo uses one window for the animated background and separate windows for
  the moving and fixed panels.
- Recomputing panel rectangles from `term_size_get()` every frame is a simple
  resize strategy.

## Related files

- [demo-window.c](/home/matt/dev/src/demo-window.c)
- [term.h](/home/matt/dev/src/term/term.h)
