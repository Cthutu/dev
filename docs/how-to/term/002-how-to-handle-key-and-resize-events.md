# How to handle key and resize events

This guide shows how to react to keyboard input and terminal resizes.

## Goal

Consume `TermEvent` values from the queue and update your UI when the terminal
changes.

## Steps

1. Start the module with `term_init()`.
2. Enter `while (term_loop())`.
3. Poll events with `term_poll_event()`.
4. Switch on `event.kind`.
5. Handle `TERM_EVENT_KEY` for input and `TERM_EVENT_RESIZE` for layout changes.

## Example

```c
//> use: core term

#include <term/term.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    TermSize current_size = {0};

    term_init();

    while (term_loop()) {
        TermEvent event = term_poll_event();

        switch (event.kind) {
        case TERM_EVENT_RESIZE:
            current_size = event.size;
            break;

        case TERM_EVENT_KEY:
            if (event.key == 'q' || event.key == 'Q') {
                term_done();
                continue;
            }
            break;

        case TERM_EVENT_NONE:
        default:
            break;
        }

        term_fb_cls(COLOUR_WHITE, COLOUR_BLACK);
        term_fb_format(2, 2, "Terminal size: %ux%u",
                       current_size.width,
                       current_size.height);
        term_fb_write_cstr(2, 4, "Resize the terminal or press q");
    }

    return 0;
}
```

## Notes

- The module queues an initial `TERM_EVENT_RESIZE` during startup, so you can
  treat resize handling as your layout initialisation path.
- `term_size_get()` is also available if you want to query the current size
  directly.
- [demo-window.c](/home/matt/dev/src/demo-window.c) recalculates panel sizes
  every frame, which is a simple way to stay resize-safe.

## Related files

- [term.h](/home/matt/dev/src/term/term.h)
- [demo-window.c](/home/matt/dev/src/demo-window.c)
