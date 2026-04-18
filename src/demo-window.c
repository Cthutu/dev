//
// demo-window.c
//
// Animated demo for the TermWindow API
//
//> desc: Animated TermWindow demo
//> use: core term

#include <term/term.h>

typedef struct {
    TermWindow backdrop;
    TermWindow main_panel;
    TermWindow probe_panel;
    TermWindow ticker_panel;
} DemoWindowState;

internal u16 demo_min_u16(u16 a, u16 b) { return a < b ? a : b; }

internal u16 demo_max_u16(u16 a, u16 b) { return a > b ? a : b; }

internal u16 demo_ping_pong(u32 tick, u16 span)
{
    if (span == 0) {
        return 0;
    }

    u32 period = (u32)span * 2;
    u32 pos    = tick % period;
    return pos < span ? (u16)pos : (u16)(period - pos);
}

internal i32 demo_signed_ping_pong(u32 tick, i32 span)
{
    if (span <= 0) {
        return 0;
    }

    u32 pos = demo_ping_pong(tick, (u16)span);
    return (i32)pos;
}

internal void
demo_window_put(TermWindow* window, int x, int y, u32 ch, u32 ink, u32 paper)
{
    if (x < 0 || y < 0 || x >= window->rect.width || y >= window->rect.height) {
        return;
    }

    usize     index = (usize)y * window->rect.width + (usize)x;
    TermCell* cell  = &window->cells[index];
    cell->ch        = ch;
    cell->ink       = ink;
    cell->paper     = paper;
}

internal void demo_window_box(
    TermWindow* window, u32 ink, u32 paper, u32 border_ink, cstr title)
{
    term_window_paint_rect(
        window,
        term_rect(0, 0, window->rect.width, window->rect.height),
        ' ',
        ink,
        paper);
    term_window_9slice(window,
                       term_rect(0, 0, window->rect.width, window->rect.height),
                       "/-\\| |\\-/",
                       false);
    term_window_paint(window,
                      term_rect(0, 0, window->rect.width, window->rect.height),
                      border_ink,
                      paper);

    if (title && title[0] != '\0' && window->rect.width > 4) {
        term_window_write_cstr(window, 2, 0, title);
    }
}

internal void demo_window_bar(TermWindow* window,
                              int         x,
                              int         y,
                              int         width,
                              u32         value,
                              u32         max_value,
                              u32         ink,
                              u32         paper)
{
    if (width <= 0) {
        return;
    }

    u32 filled =
        max_value == 0 ? 0 : (value * (u32)width + max_value / 2) / max_value;
    if (filled > (u32)width) {
        filled = (u32)width;
    }

    for (int i = 0; i < width; i++) {
        u32 ch = (u32)(i < (int)filled ? '#' : '.');
        demo_window_put(window, x + i, y, ch, ink, paper);
    }
}

internal void demo_draw_backdrop(TermWindow* window, u32 frame)
{
    static const char ramp[] = " .:-=+*#%@";
    u16               width  = window->rect.width;
    u16               height = window->rect.height;

    for (u16 y = 0; y < height; y++) {
        for (u16 x = 0; x < width; x++) {
            u32 wave    = ((u32)x * 3 + (u32)y * 5 + frame) % 10;
            u32 shimmer = ((u32)x + frame / 2) % 7;
            u32 paper   = term_rgb((u8)(10 + wave * 3),
                                 (u8)(18 + shimmer * 5),
                                 (u8)(28 + wave * 6));
            u32 ink     = term_rgb((u8)(70 + wave * 10),
                               (u8)(110 + shimmer * 12),
                               (u8)(120 + wave * 8));
            demo_window_put(
                window, x, y, (u32)(u8)ramp[(wave + shimmer) % 10], ink, paper);
        }
    }
}

internal void demo_draw_main_panel(TermWindow* window, u32 frame, TermSize size)
{
    u32 paper = term_rgb(20, 24, 32);
    u32 ink   = term_rgb(236, 223, 204);
    u32 edge  = term_rgb(255, 180, 90);

    demo_window_box(window, ink, paper, edge, " TERM WINDOW CONTROL ");

    term_window_write_cstr(
        window,
        2,
        2,
        "Layered \033[38;2;120;220;255mpanels\033[0m \033[38;5;117m◆◆\033[0m, "
        "\033[38;5;221mdirect cell painting\033[0m, and clipping.");
    term_window_write_cstr(
        window,
        2,
        3,
        "Press \033[38;5;154m[q]\033[0m to quit. "
        "\033[38;5;81mANSI resets\033[39m preserve the panel colours.");

    term_window_write_cstr(window, 2, 5, "\033[38;5;215mTerminal ◌\033[0m");
    term_window_format(
        window, 12, 5, "\033[38;5;230m%ux%u\033[0m", size.width, size.height);

    term_window_write_cstr(window, 2, 6, "\033[38;5;215mFrame ↻\033[0m");
    term_window_format(window, 12, 6, "\033[38;5;117m%u\033[0m", frame);

    term_window_write_cstr(window, 2, 8, "\033[38;5;118mCPU ▲\033[0m");
    demo_window_bar(
        window,
        12,
        8,
        demo_max_u16(10, window->rect.width > 16 ? window->rect.width - 16 : 0),
        20 + ((frame * 7) % 80),
        100,
        term_rgb(120, 255, 180),
        paper);

    term_window_write_cstr(window, 2, 9, "\033[38;5;203mGPU ■\033[0m");
    demo_window_bar(
        window,
        12,
        9,
        demo_max_u16(10, window->rect.width > 16 ? window->rect.width - 16 : 0),
        35 + ((frame * 5) % 65),
        100,
        term_rgb(255, 120, 120),
        paper);

    term_window_write_cstr(window, 2, 11, "\033[38;5;111mSignal ◇\033[0m");
    if (window->rect.width > 16 && window->rect.height > 13) {
        int plot_width  = window->rect.width - 16;
        int plot_height = demo_min_u16(6, window->rect.height - 13);

        for (int px = 0; px < plot_width; px++) {
            int py = (int)((frame + (u32)px * 3) % (u32)plot_height);
            py     = plot_height - 1 - py;
            for (int y = 0; y < plot_height; y++) {
                u32 ch     = y == py ? '*' : ' ';
                u32 ch_ink = y == py ? term_rgb(120, 220, 255) : ink;
                u32 ch_paper =
                    y == py ? term_rgb(30, 60, 70) : term_rgb(18, 20, 28);
                demo_window_put(window, 12 + px, 11 + y, ch, ch_ink, ch_paper);
            }
        }
    }
}

internal void demo_draw_probe_panel(TermWindow* window, u32 frame)
{
    u32 paper = term_rgb(14, 44, 34);
    u32 ink   = term_rgb(140, 255, 210);
    u32 edge  = term_rgb(80, 240, 180);

    demo_window_box(window, ink, paper, edge, " PROBE ");

    for (u16 y = 1; y + 1 < window->rect.height; y++) {
        for (u16 x = 1; x + 1 < window->rect.width; x++) {
            u32 grid = ((u32)x + (u32)y + frame / 3) % 5 == 0 ? '+' : '.';
            demo_window_put(window, x, y, grid, ink, paper);
        }
    }

    if (window->rect.width > 2) {
        u16 sweep_x = 1 + (frame % (window->rect.width - 2));
        for (u16 y = 1; y + 1 < window->rect.height; y++) {
            demo_window_put(window,
                            sweep_x,
                            y,
                            '|',
                            term_rgb(255, 255, 255),
                            term_rgb(40, 120, 80));
        }
    }

    if (window->rect.height > 2) {
        u16 blip_y = 1 + ((frame / 2) % (window->rect.height - 2));
        u16 blip_x =
            2 + ((frame * 3) % demo_max_u16(1, window->rect.width - 4));
        demo_window_put(window,
                        blip_x,
                        blip_y,
                        '@',
                        term_rgb(255, 250, 170),
                        term_rgb(100, 40, 20));
    }

    term_window_write_cstr(
        window,
        2,
        1,
        "\033[38;5;159mSweep ◎\033[0m "
        "\033[38;5;223mthrough clip bounds ↔↕\033[0m");
}

internal void demo_draw_ticker_panel(TermWindow* window, u32 frame)
{
    static const char* msg   = " demo-window.c  |  overlapping windows  |  "
                               "\033[38;5;216manimated clipping\033[0m  |  "
                               "\033[38;5;159mANSI window text\033[0m  |  "
                               "\033[38;5;118mreset returns to feed colours\033[0m  |  "
                               "\033[38;5;229m◆ glyphs ◇ arrows ↻ ◎\033[0m  |  ";
    u32                paper = term_rgb(42, 20, 8);
    u32                ink   = term_rgb(255, 218, 130);
    u32                edge  = term_rgb(255, 150, 70);

    demo_window_box(window, ink, paper, edge, " FEED ");

    int msg_width = (int)string_character_count(string_from_cstr(msg));
    int span      = msg_width + demo_max_u16(1, window->rect.width - 2);
    int offset  = (int)(frame % (u32)span);
    int start_x = 1 - offset;

    term_window_write_cstr(window, start_x, 1, msg);
    term_window_write_cstr(window, start_x + msg_width, 1, msg);
}

internal void demo_render(DemoWindowState* state, u32 frame)
{
    TermSize size = term_size_get();
    if (size.width < 36 || size.height < 14) {
        term_fb_cls(term_rgb(240, 240, 240), term_rgb(18, 18, 24));
        term_fb_write_cstr(2, 2, "demo-window: terminal too small");
        term_fb_format(
            2, 4, "Need at least 36x14, have %ux%u.", size.width, size.height);
        term_fb_write_cstr(2, 6, "Resize the terminal or press q.");
        term_fb_present();
        return;
    }

    u16 main_w  = demo_min_u16(64, size.width - 4);
    u16 main_h  = demo_min_u16(20, size.height - 4);
    u16 main_x  = (size.width - main_w) / 2;
    u16 main_y  = (size.height - main_h) / 2;

    u16 probe_w = demo_min_u16(30, demo_max_u16(18, size.width / 2));
    u16 probe_h = demo_min_u16(10, demo_max_u16(6, size.height / 3));
    i32 probe_x =
        demo_signed_ping_pong(frame / 2, (i32)size.width) - (i32)probe_w / 2;
    i32 probe_y =
        demo_signed_ping_pong(frame / 3, (i32)size.height) - (i32)probe_h / 2;

    u16 ticker_w = size.width;
    u16 ticker_y = size.height - 3;

    term_window_init(&state->backdrop,
                     term_rect(0, 0, size.width, size.height));
    demo_draw_backdrop(&state->backdrop, frame);

    term_window_init(
        &state->probe_panel,
        term_rect((u16)(i16)probe_x, (u16)(i16)probe_y, probe_w, probe_h));
    demo_draw_probe_panel(&state->probe_panel, frame);

    term_window_init(&state->main_panel,
                     term_rect(main_x, main_y, main_w, main_h));
    demo_draw_main_panel(&state->main_panel, frame, size);

    term_window_init(&state->ticker_panel,
                     term_rect(0, ticker_y, ticker_w, 3));
    demo_draw_ticker_panel(&state->ticker_panel, frame);

    term_fb_cls(term_rgb(255, 255, 255), term_rgb(0, 0, 0));
    term_window_draw(&state->backdrop);
    term_window_draw(&state->probe_panel);
    term_window_draw(&state->main_panel);
    term_window_draw(&state->ticker_panel);
    term_fb_present();
}

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    DemoWindowState state = {0};
    u32             frame = 0;

    term_init();
    term_cursor_hide();

    while (term_loop()) {
        TermEvent event = term_poll_event();

        if (event.kind == TERM_EVENT_KEY &&
            (event.key == 'q' || event.key == 'Q')) {
            term_done();
            continue;
        }

        demo_render(&state, frame++);
        time_sleep_ms(16);
        temp_arena_reset();
    }

    term_window_done(&state.backdrop);
    term_window_done(&state.main_panel);
    term_window_done(&state.probe_panel);
    term_window_done(&state.ticker_panel);

    return 0;
}
