# How to draw with the framebuffer

This guide covers the direct drawing API for full-screen terminal rendering.

## Goal

Clear the screen, draw coloured regions, write text, and let `term_loop()`
present the frame.

## Steps

1. Start each frame with `term_fb_cls()`.
2. Use `term_fb_rect()`, `term_fb_9slice()`, or the per-layer rectangle helpers
   to paint areas.
3. Write text with `term_fb_write_cstr()`, `term_fb_write()`, or `term_fb_format()`.
4. `term_loop()` flushes dirty framebuffer cells after your frame code runs.

## Example

```c
//> use: core term

#include <term/term.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    term_init();

    while (term_loop()) {
        TermEvent event = term_poll_event();
        if (event.kind == TERM_EVENT_KEY && event.key == 'q') {
            term_done();
            continue;
        }

        term_fb_cls(term_rgb(236, 223, 204), term_rgb(20, 24, 32));

        term_fb_rect(term_rect(2, 2, 28, 5),
                     ' ',
                     term_rgb(236, 223, 204),
                     term_rgb(40, 48, 64));
        term_fb_rect_char(term_rect(2, 2, 28, 1), '=');
        term_fb_rect_colour(term_rect(2, 6, 28, 1),
                            term_rgb(255, 180, 90),
                            term_rgb(40, 48, 64));

        term_fb_write_cstr(4, 3, "Framebuffer drawing");
        term_fb_format(4, 5, "Size: %ux%u",
                       term_size_get().width,
                       term_size_get().height);
    }

    return 0;
}
```

## Notes

- The framebuffer is clipped to the current terminal size automatically.
- `term_fb_write()` handles UTF-8 and wide glyphs.
- `term_fb_9slice()` is useful for borders and frames when you want to draw
  directly to the framebuffer without introducing a `TermWindow`.
- Framebuffer presentation is driven by `term_loop()` and still only flushes dirty cells, so changing less of the framebuffer reduces terminal output.
- The framebuffer write functions do not parse ANSI escape sequences. If you want inline ANSI colours, use `TermWindow` text APIs instead.

## Related files

- [term.h](/home/matt/dev/src/term/term.h)
- [fb.c](/home/matt/dev/src/term/fb.c)
