//> use: core term

#include <term/term.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    term_init();

    ColourTheme theme = g_themes[THEME_MONOKAI];
    term_cursor_hide();

    while (term_loop()) {
        TermEvent event = term_poll_event();

        switch (event.kind) {
        case TERM_EVENT_NONE:
            term_fb_cls(theme.ink, theme.paper);
            term_fb_format(2, 2, "Hello, World!");
            break;
        case TERM_EVENT_KEY:
            // Handle key event
            if (event.key == 'q') {
                term_done();
            }
            break;

        default:
            break;
        }
    }
    return 0;
}
