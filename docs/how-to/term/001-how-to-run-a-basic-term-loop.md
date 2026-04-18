# How to run a basic term loop

This guide shows the smallest useful program built on the `term` module.

## Goal

Initialise the terminal, render each frame, react to quit input, and shut down cleanly.

## Steps

1. Call `term_init()` once before entering the loop.
2. Optionally hide the cursor with `term_cursor_hide()`.
3. Run your frame loop with `while (term_loop())`.
4. Call `term_poll_event()` each frame to consume one queued event.
5. Render into the framebuffer and let `term_loop()` flush the pending framebuffer changes.
6. Call `term_done()` when you want the loop to stop.

## Example

```c
//> use: core term

#include <term/term.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    term_init();
    term_cursor_hide();

    while (term_loop()) {
        TermEvent event = term_poll_event();

        if (event.kind == TERM_EVENT_KEY && event.key == 'q') {
            term_done();
            continue;
        }

        term_fb_cls(COLOUR_WHITE, COLOUR_BLACK);
        term_fb_write_cstr(2, 2, "Hello from term");
        term_fb_write_cstr(2, 4, "Press q to quit");
    }

    return 0;
}
```

## Notes

- `term_done()` sets the module's running flag. The terminal cleanup happens as the loop exits.
- `term_poll_event()` returns one event at a time. If the queue is empty, you get `TERM_EVENT_NONE`.
- This is the same control flow used by [demo-term.c](/home/matt/dev/src/demo-term.c).

## Related files

- [term.h](/home/matt/dev/src/term/term.h)
- [demo-term.c](/home/matt/dev/src/demo-term.c)
