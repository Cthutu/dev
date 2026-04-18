# How to use ANSI colour text in a window

This guide covers the inline ANSI colour support implemented by `term_window_write()`.

## Goal

Write styled text into a `TermWindow` without manually changing each cell.

## Steps

1. Paint the destination area with its base colours.
2. Prefer the ANSI macros from `core.h` for basic styles and colours.
3. Use raw SGR sequences when you need ANSI 256-colour or true-colour values.
4. Use `ANSI_RESET`, `ANSI_FG_RESET`, or `ANSI_BG_RESET` to reset back to the
   window's base colours.
5. Draw the window normally with `term_window_draw()`.

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

        term_window_init(&panel, term_rect(2, 2, 50, 6));
        term_window_paint_rect(&panel,
                               term_rect(0, 0, panel.rect.width, panel.rect.height),
                               ' ',
                               term_rgb(236, 223, 204),
                               term_rgb(20, 24, 32));

        term_window_write_cstr(
            &panel,
            2,
            2,
            "Normal "
            ANSI_BOLD_CYAN "macro" ANSI_RESET " "
            "\033[38;5;117m256-colour" ANSI_RESET " "
            "\033[38;2;255;180;90mRGB" ANSI_RESET " "
            "\033[48;5;52mbackground" ANSI_BG_RESET " "
            "back to panel colours");

        term_fb_cls(COLOUR_WHITE, COLOUR_BLACK);
        term_window_draw(&panel);
        term_fb_present();

        term_window_done(&panel);
    }

    return 0;
}
```

## Notes

- `core.h` already provides common ANSI helpers such as `ANSI_RESET`,
  `ANSI_FG_RESET`, `ANSI_BG_RESET`, `ANSI_BOLD`, `ANSI_RED`, and
  `ANSI_BG_BLUE`.
- The window writer supports standard foreground and background SGR colours,
  bright colours, ANSI 256-colour values, and `38;2` / `48;2` true-colour
  sequences.
- Reset codes restore the colours already present in the destination window
  cells before the write began.
- The `core.h` macros cover the common fixed ANSI styles. The raw `\033[...]`
  form is still useful when you need 256-colour or true-colour control.
- This behaviour is used heavily in [demo-window.c](/home/matt/dev/src/demo-window.c).
- Inline ANSI parsing is a `TermWindow` feature. The framebuffer text writers do
  not parse escape sequences.

## Related files

- [core.h](/home/matt/dev/src/core/core.h)
- [window.c](/home/matt/dev/src/term/window.c)
- [demo-window.c](/home/matt/dev/src/demo-window.c)
