# How to read line input from TermConsole

This guide shows how to accept input lines, echo them back to the console, and
trigger behaviour from the submitted text.

## Goal

Poll `term_console_get_input()` after forwarding events so you only act when the
user presses ENTER.

## Steps

1. Build your `TermConsole`.
2. Pass terminal events into `term_console_send_event()`.
3. Call `term_console_get_input()` each frame.
4. Use the returned `string` immediately or copy it elsewhere if you need to
   keep it.

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

    while (term_loop()) {
        TermEvent event = term_poll_event();
        if (event.kind != TERM_EVENT_NONE) {
            term_console_send_event(&console, event);
        }

        string input = {0};
        if (term_console_get_input(&console, &input)) {
            if (string_equals_cstr(input, "q")) {
                term_done();
                continue;
            }

            term_console_format(&console, "echo: " STRINGP, STRINGV(input));
        }
    }

    term_console_done(&console);
    term_window_done(&window);
    return 0;
}
```

## Notes

- `term_console_get_input()` returns `true` once per accepted line.
- The returned `string` points into console-owned storage and remains valid
  until the next accepted line or `term_console_done()`.
- `ENTER` accepts the current line and also snaps the console view back to the
  latest output if the user had scrolled up.
- [demo-console.c](/home/matt/dev/src/demo-console.c) combines input echoing
  with periodic output.

## Related files

- [term.h](/home/matt/dev/src/term/term.h)
- [console.c](/home/matt/dev/src/term/console.c)
- [demo-console.c](/home/matt/dev/src/demo-console.c)
