# How to build a console with TermConsole

This guide shows how to connect `TermConsole` to a full-screen `TermWindow`.

## Goal

Create a console that matches the terminal size, redraws on resize, and keeps a
scrollable output history with an editable input line.

## Steps

1. Call `term_init()`.
2. Query the initial size with `term_size_get()`.
3. Create a `TermWindow` that matches that size.
4. Initialise `TermConsole` with the window, then optionally set prompt, output,
   and input colours.
5. Forward every `TermEvent` into `term_console_send_event()`.
6. Let `term_loop()` present the pending framebuffer changes.

## Example

```c
//> use: core term

#include <term/term.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    term_init();

    TermSize size = term_size_get();
    TermWindow window = {0};
    TermConsole console = {0};

    term_window_init(&window, term_rect(0, 0, size.width, size.height));
    term_console_init(&console, &window, 256);
    term_console_set_prompt(&console, S("> "));
    term_console_write_cstr(&console, "Console ready");

    while (term_loop()) {
        TermEvent event = term_poll_event();
        if (event.kind != TERM_EVENT_NONE) {
            term_console_send_event(&console, event);
        }
    }

    term_console_done(&console);
    term_window_done(&window);
    return 0;
}
```

## Notes

- `term_console_send_event()` handles both key input and `TERM_EVENT_RESIZE`.
- `term_console_send_event()` also handles mouse wheel scrolling and implicitly
  moves console focus to the console receiving the event.
- `term_console_resize()` calls `term_window_resize()` underneath, so the
  backing window stays in sync with the terminal.
- `history_size` limits how many output chunks the console keeps.
- Use `term_console_set_output_colour()`, `term_console_set_prompt_colour()`,
  and `term_console_set_input_colour()` if you want colours other than the
  default white-on-black console output.

## Related files

- [term.h](/home/matt/dev/src/term/term.h)
- [console.c](/home/matt/dev/src/term/console.c)
- [demo-console.c](/home/matt/dev/src/demo-console.c)
