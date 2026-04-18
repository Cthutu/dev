//------------------------------------------------------------------------------
// Terminal console implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include "internal.h"

//------------------------------------------------------------------------------

global_variable TermConsole* g_term_focused_console = NULL;

//------------------------------------------------------------------------------

internal void _term_console_redraw(TermConsole* console);

//------------------------------------------------------------------------------

internal void _term_console_focus(TermConsole* console)
{
    if (!console->input_enabled) {
        return;
    }

    if (g_term_focused_console && g_term_focused_console != console) {
        g_term_focused_console->focused = false;
        _term_console_redraw(g_term_focused_console);
    }

    g_term_focused_console = console;
    console->focused       = true;
}

//------------------------------------------------------------------------------

internal u32 _term_console_ansi_palette(int index)
{
    static const u32 palette[16] = {
        COLOUR_BLACK,
        COLOUR_RED,
        COLOUR_GREEN,
        COLOUR_YELLOW,
        COLOUR_BLUE,
        COLOUR_MAGENTA,
        COLOUR_CYAN,
        COLOUR_BRIGHT_GREY,
        COLOUR_GREY,
        COLOUR_BRIGHT_RED,
        COLOUR_BRIGHT_GREEN,
        COLOUR_BRIGHT_YELLOW,
        COLOUR_BRIGHT_BLUE,
        COLOUR_BRIGHT_MAGENTA,
        COLOUR_BRIGHT_CYAN,
        COLOUR_WHITE,
    };

    index = CLAMP(index, 0, 15);
    return palette[index];
}

internal u32 _term_console_ansi_256_colour(int index)
{
    static const u8 cube_levels[6] = {0, 95, 135, 175, 215, 255};

    if (index < 16) {
        return _term_console_ansi_palette(index);
    }

    if (index < 232) {
        int base = index - 16;
        return term_rgb(cube_levels[base / 36],
                        cube_levels[(base / 6) % 6],
                        cube_levels[base % 6]);
    }

    index = CLAMP(index, 232, 255);
    u8 grey = (u8)(8 + ((index - 232) * 10));
    return term_rgb(grey, grey, grey);
}

internal void _term_console_apply_sgr(int  params[],
                                      int  count,
                                      u32  base_ink,
                                      u32  base_paper,
                                      u32* io_ink,
                                      u32* io_paper)
{
    if (count == 0) {
        *io_ink   = base_ink;
        *io_paper = base_paper;
        return;
    }

    for (int i = 0; i < count; i++) {
        int code = params[i];

        if (code == 0) {
            *io_ink   = base_ink;
            *io_paper = base_paper;
        } else if (code >= 30 && code <= 37) {
            *io_ink = _term_console_ansi_palette(code - 30);
        } else if (code >= 90 && code <= 97) {
            *io_ink = _term_console_ansi_palette((code - 90) + 8);
        } else if (code >= 40 && code <= 47) {
            *io_paper = _term_console_ansi_palette(code - 40);
        } else if (code >= 100 && code <= 107) {
            *io_paper = _term_console_ansi_palette((code - 100) + 8);
        } else if (code == 39) {
            *io_ink = base_ink;
        } else if (code == 49) {
            *io_paper = base_paper;
        } else if ((code == 38 || code == 48) && i + 1 < count) {
            u32* target = code == 38 ? io_ink : io_paper;
            int  mode   = params[++i];

            if (mode == 5 && i + 1 < count) {
                *target = _term_console_ansi_256_colour(params[++i]);
            } else if (mode == 2 && i + 3 < count) {
                int r   = CLAMP(params[++i], 0, 255);
                int g   = CLAMP(params[++i], 0, 255);
                int b   = CLAMP(params[++i], 0, 255);
                *target = term_rgb((u8)r, (u8)g, (u8)b);
            }
        }
    }
}

internal usize _term_console_try_parse_sgr(const u8* s,
                                           const u8* end,
                                           u32       base_ink,
                                           u32       base_paper,
                                           u32*      io_ink,
                                           u32*      io_paper)
{
    int       params[16];
    int       count      = 0;
    int       current    = 0;
    bool      have_digit = false;
    const u8* p          = s;

    if ((usize)(end - s) < 2 || p[0] != '\033' || p[1] != '[') {
        return 0;
    }
    p += 2;

    while (p < end) {
        u8 ch = *p;
        if (ch >= '0' && ch <= '9') {
            current    = current * 10 + (int)(ch - '0');
            have_digit = true;
            p++;
        } else if (ch == ';') {
            if (count < ARRAY_COUNT(params)) {
                params[count++] = have_digit ? current : 0;
            }
            current    = 0;
            have_digit = false;
            p++;
        } else if (ch == 'm') {
            if (count < ARRAY_COUNT(params)) {
                params[count++] = have_digit ? current : 0;
            }
            _term_console_apply_sgr(
                params, count, base_ink, base_paper, io_ink, io_paper);
            return (usize)((p + 1) - s);
        } else {
            return 0;
        }
    }

    return 0;
}

internal string _term_console_dup_string(string str)
{
    string copy = {0};
    if (str.count == 0) {
        return copy;
    }

    copy.data  = ARRAY_ALLOC(u8, str.count);
    copy.count = str.count;
    memcpy(copy.data, str.data, str.count);
    return copy;
}

internal void _term_console_set_bytes(Array(u8)* bytes, string str)
{
    array_free(*bytes);
    if (str.count == 0) {
        return;
    }

    array_requires_size(*bytes, str.count);
    memcpy(*bytes, str.data, str.count);
}

internal void _term_console_discard_oldest(TermConsole* console)
{
    while (console->history_size > 0 &&
           array_count(console->history) > console->history_size) {
        FREE(console->history[0].text.data);
        array_delete(console->history, 0);
    }
}

internal void _term_console_push_history(TermConsole* console,
                                         string       str,
                                         bool         wrap)
{
    if (str.count == 0) {
        return;
    }

    array_push(console->history,
               ((TermConsoleChunk){.text = _term_console_dup_string(str),
                                   .wrap = wrap}));
    _term_console_discard_oldest(console);
}

internal void _term_console_put_cell(
    TermWindow* window, int x, int y, u32 ch, usize width, u32 ink, u32 paper)
{
    if (x < 0 || y < 0 || x >= (int)window->rect.width ||
        y >= (int)window->rect.height) {
        return;
    }

    usize index                = (usize)y * window->rect.width + (usize)x;
    window->cells[index].ch    = ch;
    window->cells[index].ink   = ink;
    window->cells[index].paper = paper;

    for (usize i = 1; i < width && x + (int)i < (int)window->rect.width; i++) {
        TermCell* tail = &window->cells[index + i];
        tail->ch       = TERM_FB_CHAR_WIDE_TAIL;
        tail->ink      = ink;
        tail->paper    = paper;
    }
}

internal void _term_console_measure_output(const TermConsole* console,
                                           int                width,
                                           usize*             out_rows)
{
    usize rows = 0;
    bool  any  = false;

    for (usize i = 0; i < array_count(console->history); i++) {
        TermConsoleChunk chunk         = console->history[i];
        const u8*        s             = chunk.text.data;
        const u8*        end           = chunk.text.data + chunk.text.count;
        int              col           = 0;
        u32              current_ink   = console->default_ink;
        u32              current_paper = console->default_paper;

        while (s < end) {
            usize sgr_bytes = _term_console_try_parse_sgr(s,
                                                          end,
                                                          console->default_ink,
                                                          console->default_paper,
                                                          &current_ink,
                                                          &current_paper);
            if (sgr_bytes != 0) {
                any = true;
                s += sgr_bytes;
                continue;
            }

            cstr  cursor = (cstr)s;
            u32   ch;
            usize bytes;
            usize glyph_width;
            term_utf8_next(&cursor, &ch, &bytes, &glyph_width);
            UNUSED(bytes);
            s = (const u8*)cursor;

            any = true;
            if (ch == '\n') {
                rows++;
                col = 0;
                continue;
            }

            glyph_width = MAX(glyph_width, 1);

            if (width > 0 && chunk.wrap && col > 0 &&
                col + (int)glyph_width > width) {
                rows++;
                col = 0;
            }

            col += (int)glyph_width;
        }

        if (col != 0) {
            rows++;
            col = 0;
        }
    }

    *out_rows = any ? rows : 0;
}

internal usize _term_console_input_rows(const TermConsole* console, int width)
{
    if (!console->input_enabled || width <= 0) {
        return 0;
    }

    usize cols = array_count(console->prompt) + array_count(console->input);
    return MAX((cols + (usize)width - 1) / (usize)width, 1);
}

internal void _term_console_draw_output(const TermConsole* console,
                                        usize              start_row,
                                        usize              visible_rows)
{
    TermWindow* window = console->window;
    int         width  = (int)window->rect.width;
    if (width <= 0 || visible_rows == 0) {
        return;
    }

    usize row           = 0;
    int   col           = 0;
    u32   current_ink   = console->default_ink;
    u32   current_paper = console->default_paper;

    for (usize i = 0; i < array_count(console->history); i++) {
        TermConsoleChunk chunk = console->history[i];
        const u8*        s     = chunk.text.data;
        const u8*        end   = chunk.text.data + chunk.text.count;

        while (s < end) {
            usize sgr_bytes = _term_console_try_parse_sgr(s,
                                                          end,
                                                          console->default_ink,
                                                          console->default_paper,
                                                          &current_ink,
                                                          &current_paper);
            if (sgr_bytes != 0) {
                s += sgr_bytes;
                continue;
            }

            cstr  cursor = (cstr)s;
            u32   ch;
            usize bytes;
            usize glyph_width;
            term_utf8_next(&cursor, &ch, &bytes, &glyph_width);
            UNUSED(bytes);
            s = (const u8*)cursor;

            if (ch == '\n') {
                row++;
                col = 0;
                continue;
            }

            glyph_width = MAX(glyph_width, 1);

            if (chunk.wrap && col > 0 && col + (int)glyph_width > width) {
                row++;
                col = 0;
            }

            if (row >= start_row && row < start_row + visible_rows &&
                col >= 0 && col + (int)glyph_width <= width) {
                _term_console_put_cell(window,
                                       col,
                                       (int)(row - start_row),
                                       ch,
                                       glyph_width,
                                       current_ink,
                                       current_paper);
            }

            col += (int)glyph_width;
        }

        if (col != 0) {
            row++;
            col = 0;
        }
    }
}

internal void _term_console_draw_input(TermConsole* console, usize start_row)
{
    if (!console->input_enabled) {
        return;
    }

    TermWindow* window = console->window;
    int         width  = (int)window->rect.width;
    if (width <= 0 || start_row >= window->rect.height) {
        return;
    }

    usize index = 0;
    int   x     = 0;
    int   y     = (int)start_row;
    usize total = array_count(console->prompt) + array_count(console->input);

    while (index < total && y < (int)window->rect.height) {
        u8 ch = index < array_count(console->prompt)
                    ? console->prompt[index]
                    : console->input[index - array_count(console->prompt)];
        _term_console_put_cell(window,
                               x,
                               y,
                               ch,
                               1,
                               console->default_ink,
                               console->default_paper);
        index++;
        x++;
        if (x >= width) {
            x = 0;
            y++;
        }
    }

    if (g_term_focused_console == console && console->input_enabled) {
        g_cursor_visible = true;
        g_cursor_ink     = console->default_ink;
        g_cursor_paper   = console->default_paper;
        g_cursor_x       = (int)window->rect.x + x;
        g_cursor_y       = (int)window->rect.y + y;
    }
}

internal void _term_console_redraw(TermConsole* console)
{
    TermWindow* window = console->window;
    int         width  = (int)window->rect.width;
    int         height = (int)window->rect.height;

    term_window_clear(
        window, ' ', console->default_ink, console->default_paper);

    if (width <= 0 || height <= 0) {
        term_window_draw(window);
        return;
    }

    usize output_rows = 0;
    usize input_rows  = _term_console_input_rows(console, width);
    _term_console_measure_output(console, width, &output_rows);

    usize input_start = output_rows;
    if (input_start + input_rows > (usize)height) {
        input_start = (usize)height - MIN(input_rows, (usize)height);
    }

    usize visible_output_rows = MIN(input_start, (usize)height);
    usize max_scroll          = output_rows > visible_output_rows
                                    ? output_rows - visible_output_rows
                                    : 0;
    console->scroll_offset    = MIN(console->scroll_offset, max_scroll);
    usize output_start        = output_rows > visible_output_rows
                                    ? output_rows - visible_output_rows -
                                          console->scroll_offset
                                    : 0;

    _term_console_draw_output(console, output_start, visible_output_rows);
    _term_console_draw_input(console, input_start);
    term_window_draw(window);
}

//------------------------------------------------------------------------------

void term_console_init(TermConsole* console,
                       TermWindow*  window,
                       u32          default_ink,
                       u32          default_paper,
                       u32          history_size)
{
    memset(console, 0, sizeof(*console));
    console->window         = window;
    console->default_ink    = default_ink;
    console->default_paper  = default_paper;
    console->history_size   = history_size;
    console->input_enabled  = true;
    console->focused        = false;
    console->scroll_offset  = 0;
    _term_console_redraw(console);
}

void term_console_done(TermConsole* console)
{
    if (g_term_focused_console == console) {
        g_term_focused_console = NULL;
        console->focused       = false;
    }

    for (usize i = 0; i < array_count(console->history); i++) {
        FREE(console->history[i].text.data);
    }
    array_free(console->history);
    array_free(console->prompt);
    array_free(console->input);
    array_free(console->pending_input);
    memset(console, 0, sizeof(*console));
}

void term_console_enable_input(TermConsole* console, bool enable)
{
    console->input_enabled = enable;
    if (!enable) {
        array_clear(console->input);
        term_console_unfocus(console);
        return;
    }
    _term_console_redraw(console);
}

void term_console_set_prompt(TermConsole* console, string prompt)
{
    _term_console_set_bytes(&console->prompt, prompt);
    _term_console_redraw(console);
}

void term_console_resize(TermConsole* console, TermRect new_rect)
{
    term_window_resize(console->window, new_rect);
    console->scroll_offset = 0;
    _term_console_redraw(console);
}

void term_console_clear(TermConsole* console)
{
    for (usize i = 0; i < array_count(console->history); i++) {
        FREE(console->history[i].text.data);
    }
    array_clear(console->history);
    console->scroll_offset = 0;
    _term_console_redraw(console);
}

void term_console_write(TermConsole* console, string str)
{
    _term_console_push_history(console, str, false);
    console->scroll_offset = 0;
    _term_console_redraw(console);
}

void term_console_write_cstr(TermConsole* console, cstr string)
{
    term_console_write(console, string_from_cstr(string));
}

void term_console_write_wrap(TermConsole* console, string str)
{
    _term_console_push_history(console, str, true);
    console->scroll_offset = 0;
    _term_console_redraw(console);
}

void term_console_formatv(TermConsole* console, cstr fmt, va_list args)
{
    string formatted = string_formatv(temp_arena(), fmt, args);
    term_console_write(console, formatted);
}

void term_console_format(TermConsole* console, cstr fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    term_console_formatv(console, fmt, args);
    va_end(args);
}

void term_console_formatv_wrap(TermConsole* console, cstr fmt, va_list args)
{
    string formatted = string_formatv(temp_arena(), fmt, args);
    term_console_write_wrap(console, formatted);
}

void term_console_format_wrap(TermConsole* console, cstr fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    term_console_formatv_wrap(console, fmt, args);
    va_end(args);
}

void term_console_send_event(TermConsole* console, TermEvent event)
{
    _term_console_focus(console);

    switch (event.kind) {
    case TERM_EVENT_NONE:
        break;

    case TERM_EVENT_RESIZE:
        term_console_resize(console,
                            term_rect(console->window->rect.x,
                                      console->window->rect.y,
                                      event.size.width,
                                      event.size.height));
        return;

    case TERM_EVENT_KEY:
        if (!console->input_enabled) {
            break;
        }

        if (event.key == '\r' || event.key == '\n') {
            _term_console_set_bytes(
                &console->pending_input,
                string_from(console->input, array_count(console->input)));
            console->has_pending_input = true;
            array_clear(console->input);
        } else if (event.key == 8 || event.key == 127) {
            if (array_count(console->input) > 0) {
                __array_count(console->input)--;
            }
        } else if ((u8)event.key >= 32) {
            array_push(console->input, (u8)event.key);
        }
        break;
    }

    _term_console_redraw(console);
}

void term_console_unfocus(TermConsole* console)
{
    if (g_term_focused_console == console) {
        g_term_focused_console = NULL;
    }
    console->focused = false;
    term_cursor_hide();
    _term_console_redraw(console);
}

bool term_console_get_input(TermConsole* console, string* out_input)
{
    if (!console->has_pending_input) {
        return false;
    }

    if (out_input) {
        *out_input =
            string_from(console->pending_input, array_count(console->pending_input));
    }
    console->has_pending_input = false;
    return true;
}

//------------------------------------------------------------------------------
