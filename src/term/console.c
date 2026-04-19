//------------------------------------------------------------------------------
// Terminal console implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include "internal.h"
#include <ctype.h>

//------------------------------------------------------------------------------

global_variable TermConsole* g_term_focused_console = NULL;

//------------------------------------------------------------------------------

typedef struct {
    usize output_rows;
    usize input_rows;
    usize visible_output_rows;
    usize max_scroll;
    int   output_width;
    bool  show_scrollbar;
} TermConsoleLayout;

//------------------------------------------------------------------------------

internal void              _term_console_redraw(TermConsole* console);
internal TermConsoleLayout _term_console_layout(const TermConsole* console);
internal void   _term_console_load_input(TermConsole* console, string str);
internal string _term_console_dup_string(string str);
internal void   _term_console_set_bytes(Array(u8) * bytes, string str);

//------------------------------------------------------------------------------
// _term_console_is_word_byte
//
// Test whether a byte should be treated as part of a word during cursor motion
// and deletion commands.
//------------------------------------------------------------------------------

internal bool _term_console_is_word_byte(u8 ch)
{
    return isalnum((unsigned char)ch) || ch == '_';
}

//------------------------------------------------------------------------------
// _term_console_insert_byte
//
// Insert a byte into the editable input buffer at the current cursor position.
//------------------------------------------------------------------------------

internal void _term_console_insert_byte(TermConsole* console, u8 ch)
{
    usize count = array_count(console->input);
    usize pos   = MIN(console->input_cursor, count);

    array_push(console->input, 0);
    if (pos < count) {
        memmove(console->input + pos + 1, console->input + pos, count - pos);
    }
    console->input[pos]   = ch;
    console->input_cursor = pos + 1;
}

//------------------------------------------------------------------------------
// _term_console_delete_range
//
// Delete a half-open range from the editable input buffer.
//------------------------------------------------------------------------------

internal void
_term_console_delete_range(TermConsole* console, usize start, usize end)
{
    usize count = array_count(console->input);
    start       = MIN(start, count);
    end         = MIN(end, count);
    if (end <= start) {
        return;
    }

    memmove(console->input + start, console->input + end, count - end);
    __array_count(console->input) -= end - start;
    console->input_cursor = start;
}

//------------------------------------------------------------------------------
// _term_console_word_left
//
// Find the previous word boundary from the supplied cursor position.
//------------------------------------------------------------------------------

internal usize _term_console_word_left(const TermConsole* console, usize cursor)
{
    usize pos = MIN(cursor, array_count(console->input));

    while (pos > 0 && isspace((unsigned char)console->input[pos - 1])) {
        pos--;
    }
    while (pos > 0 && _term_console_is_word_byte(console->input[pos - 1])) {
        pos--;
    }
    while (pos > 0 && !isspace((unsigned char)console->input[pos - 1]) &&
           !_term_console_is_word_byte(console->input[pos - 1])) {
        pos--;
    }

    return pos;
}

//------------------------------------------------------------------------------
// _term_console_word_right
//
// Find the next word boundary from the supplied cursor position.
//------------------------------------------------------------------------------

internal usize _term_console_word_right(const TermConsole* console,
                                        usize              cursor)
{
    usize count = array_count(console->input);
    usize pos   = MIN(cursor, count);

    if (pos < count && _term_console_is_word_byte(console->input[pos])) {
        while (pos < count && _term_console_is_word_byte(console->input[pos])) {
            pos++;
        }
    } else if (pos < count && !isspace((unsigned char)console->input[pos])) {
        while (pos < count && !isspace((unsigned char)console->input[pos]) &&
               !_term_console_is_word_byte(console->input[pos])) {
            pos++;
        }
    }
    while (pos < count && isspace((unsigned char)console->input[pos])) {
        pos++;
    }

    return pos;
}

//------------------------------------------------------------------------------
// _term_console_load_history_entry
//
// Load either the currently selected history item or the saved draft into the
// editable input buffer.
//------------------------------------------------------------------------------

internal void _term_console_load_history_entry(TermConsole* console)
{
    if (console->input_history_index < 0) {
        _term_console_load_input(
            console,
            string_from(console->input_history_saved,
                        array_count(console->input_history_saved)));
        return;
    }

    usize index = (usize)console->input_history_index;
    if (index < array_count(console->input_history)) {
        _term_console_load_input(console, console->input_history[index]);
    }
}

//------------------------------------------------------------------------------
// _term_console_history_up
//
// Move backward through input history, saving the current draft on first use.
//------------------------------------------------------------------------------

internal void _term_console_history_up(TermConsole* console)
{
    usize count = array_count(console->input_history);
    if (count == 0) {
        return;
    }

    if (console->input_history_index < 0) {
        _term_console_set_bytes(
            &console->input_history_saved,
            string_from(console->input, array_count(console->input)));
        console->input_history_index = (isize)count - 1;
    } else if (console->input_history_index > 0) {
        console->input_history_index--;
    }

    _term_console_load_history_entry(console);
}

//------------------------------------------------------------------------------
// _term_console_history_down
//
// Move forward through input history, restoring the saved draft at the end.
//------------------------------------------------------------------------------

internal void _term_console_history_down(TermConsole* console)
{
    if (console->input_history_index < 0) {
        return;
    }

    if ((usize)(console->input_history_index + 1) <
        array_count(console->input_history)) {
        console->input_history_index++;
    } else {
        console->input_history_index = -1;
    }

    _term_console_load_history_entry(console);
}

//------------------------------------------------------------------------------
// _term_console_load_input
//
// Replace the editable input buffer and move the cursor to its end.
//------------------------------------------------------------------------------

internal void _term_console_load_input(TermConsole* console, string str)
{
    _term_console_set_bytes(&console->input, str);
    console->input_cursor = array_count(console->input);
}

//------------------------------------------------------------------------------
// _term_console_accept_input
//
// Commit the current input line and reset editing state for the next prompt.
//------------------------------------------------------------------------------

internal void _term_console_accept_input(TermConsole* console)
{
    console->auto_scroll   = true;
    console->scroll_offset = 0;
    _term_console_set_bytes(
        &console->pending_input,
        string_from(console->input, array_count(console->input)));
    console->has_pending_input = true;

    if (array_count(console->input) > 0) {
        string copy = _term_console_dup_string(
            string_from(console->input, array_count(console->input)));
        array_push(console->input_history, copy);
        while (console->history_size > 0 &&
               array_count(console->input_history) > console->history_size) {
            FREE(console->input_history[0].data);
            array_delete(console->input_history, 0);
        }
    }

    array_clear(console->input);
    console->input_cursor        = 0;
    console->input_history_index = -1;
    array_clear(console->input_history_saved);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_console_focus
//
// Make this console the globally focused console so it owns the host cursor.
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

//------------------------------------------------------------------------------
// _term_console_dup_string
//
// Allocate and return an owned copy of the supplied string.
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// _term_console_set_bytes
//
// Replace a dynamic byte buffer with a copy of the supplied string contents.
//------------------------------------------------------------------------------

internal void _term_console_set_bytes(Array(u8) * bytes, string str)
{
    array_done(*bytes);
    if (str.count == 0) {
        return;
    }

    array_requires_size(*bytes, str.count);
    memcpy(*bytes, str.data, str.count);
}

//------------------------------------------------------------------------------
// _term_console_discard_oldest
//
// Trim output history chunks so they stay within the configured limit.
//------------------------------------------------------------------------------

internal void _term_console_discard_oldest(TermConsole* console)
{
    while (console->history_size > 0 &&
           array_count(console->history) > console->history_size) {
        FREE(console->history[0].text.data);
        array_delete(console->history, 0);
    }
}

//------------------------------------------------------------------------------
// _term_console_push_history
//
// Append a chunk to the output history and enforce the history limit.
//------------------------------------------------------------------------------

internal void
_term_console_push_history(TermConsole* console, string str, bool wrap)
{
    if (str.count == 0) {
        return;
    }

    array_push(console->history,
               ((TermConsoleChunk){.text = _term_console_dup_string(str),
                                   .wrap = wrap}));
    _term_console_discard_oldest(console);
}

//------------------------------------------------------------------------------
// _term_console_append_history
//
// Append output while preserving the current viewport position when auto-scroll
// is paused.
//------------------------------------------------------------------------------

internal void
_term_console_append_history(TermConsole* console, string str, bool wrap)
{
    TermConsoleLayout layout_before = {0};
    if (!console->auto_scroll) {
        layout_before = _term_console_layout(console);
    }

    _term_console_push_history(console, str, wrap);

    if (!console->auto_scroll) {
        TermConsoleLayout layout_after = _term_console_layout(console);
        if (layout_after.output_rows > layout_before.output_rows) {
            console->scroll_offset +=
                layout_after.output_rows - layout_before.output_rows;
            console->scroll_offset =
                MIN(console->scroll_offset, layout_after.max_scroll);
        }
    } else {
        console->scroll_offset = 0;
    }
}

//------------------------------------------------------------------------------
// _term_console_put_cell
//
// Write a glyph into the window buffer, including wide-character tail cells.
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// _term_console_measure_output
//
// Measure the number of visual rows occupied by the current output history.
//------------------------------------------------------------------------------

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
        u32              current_ink   = console->output_colour;
        u32              current_paper = COLOUR_BLACK;

        while (s < end) {
            usize sgr_bytes = _term_try_parse_sgr(s,
                                                  end,
                                                  console->output_colour,
                                                  COLOUR_BLACK,
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
            s   = (const u8*)cursor;

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

        current_ink   = console->output_colour;
        current_paper = COLOUR_BLACK;
    }

    *out_rows = any ? rows : 0;
}

//------------------------------------------------------------------------------
// _term_console_input_rows
//
// Compute how many rows are needed for the prompt and editable input.
//------------------------------------------------------------------------------

internal usize _term_console_input_rows(const TermConsole* console, int width)
{
    if (!console->input_enabled || width <= 0) {
        return 0;
    }

    usize cols = array_count(console->prompt) + array_count(console->input);
    return MAX((cols + (usize)width - 1) / (usize)width, 1);
}

//------------------------------------------------------------------------------
// _term_console_draw_output
//
// Draw the visible slice of output history into the backing window.
//------------------------------------------------------------------------------

internal void _term_console_draw_output(const TermConsole* console,
                                        usize              start_row,
                                        usize              visible_rows,
                                        int                width)
{
    TermWindow* window = console->window;
    if (width <= 0 || visible_rows == 0) {
        return;
    }

    usize row = 0;
    int   col = 0;

    for (usize i = 0; i < array_count(console->history); i++) {
        TermConsoleChunk chunk         = console->history[i];
        const u8*        s             = chunk.text.data;
        const u8*        end           = chunk.text.data + chunk.text.count;
        u32              current_ink   = console->output_colour;
        u32              current_paper = COLOUR_BLACK;

        while (s < end) {
            usize sgr_bytes = _term_try_parse_sgr(s,
                                                  end,
                                                  console->output_colour,
                                                  COLOUR_BLACK,
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

//------------------------------------------------------------------------------
// _term_console_draw_input
//
// Draw the prompt and editable input line and place the host cursor when this
// console has focus.
//------------------------------------------------------------------------------

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
    usize cursor_total = array_count(console->prompt) + console->input_cursor;

    while (index < total && y < (int)window->rect.height) {
        u8  ch  = index < array_count(console->prompt)
                      ? console->prompt[index]
                      : console->input[index - array_count(console->prompt)];
        u32 ink = index < array_count(console->prompt) ? console->prompt_colour
                                                       : console->input_colour;
        _term_console_put_cell(window, x, y, ch, 1, ink, COLOUR_BLACK);
        index++;
        x++;
        if (x >= width) {
            x = 0;
            y++;
        }
    }

    x = 0;
    y = (int)start_row;
    for (usize cursor_index = 0;
         cursor_index < cursor_total && y < (int)window->rect.height;
         cursor_index++) {
        x++;
        if (x >= width) {
            x = 0;
            y++;
        }
    }

    if (g_term_focused_console == console && console->input_enabled) {
        g_cursor_visible = true;
        g_cursor_ink     = console->input_colour;
        g_cursor_paper   = COLOUR_BLACK;
        g_cursor_x       = (int)window->rect.x + x;
        g_cursor_y       = (int)window->rect.y + y;
    }
}

//------------------------------------------------------------------------------
// _term_console_draw_scrollbar
//
// Draw the scrollbar when the viewport is scrolled away from the live bottom.
//------------------------------------------------------------------------------

internal void _term_console_draw_scrollbar(const TermConsole* console,
                                           TermConsoleLayout  layout)
{
    if (!layout.show_scrollbar || layout.visible_output_rows == 0) {
        return;
    }

    TermWindow* window = console->window;
    int         x      = (int)window->rect.width - 1;
    usize       rows   = layout.visible_output_rows;
    usize       thumb_height =
        MAX((rows * rows) / MAX(layout.output_rows, (usize)1), (usize)1);
    usize thumb_range = rows > thumb_height ? rows - thumb_height : 0;
    usize thumb_y =
        layout.max_scroll > 0
            ? ((layout.max_scroll - console->scroll_offset) * thumb_range) /
                  layout.max_scroll
            : 0;

    for (usize y = 0; y < rows; y++) {
        _term_console_put_cell(
            window, x, (int)y, 0x2591, 1, term_rgb(80, 70, 110), COLOUR_BLACK);
    }

    for (usize y = thumb_y; y < thumb_y + thumb_height && y < rows; y++) {
        _term_console_put_cell(
            window, x, (int)y, 0x2588, 1, term_rgb(255, 180, 80), COLOUR_BLACK);
    }
}

//------------------------------------------------------------------------------
// _term_console_layout
//
// Calculate the output, input, and scrollbar layout for the current window
// size and scroll state.
//------------------------------------------------------------------------------

internal TermConsoleLayout _term_console_layout(const TermConsole* console)
{
    TermConsoleLayout layout = {0};
    int               width  = (int)console->window->rect.width;
    int               height = (int)console->window->rect.height;

    if (width <= 0 || height <= 0) {
        return layout;
    }

    layout.show_scrollbar = console->scroll_offset > 0;
    layout.output_width   = width - (layout.show_scrollbar ? 1 : 0);
    layout.output_width   = MAX(layout.output_width, 0);
    layout.input_rows     = _term_console_input_rows(console, width);
    _term_console_measure_output(
        console, layout.output_width, &layout.output_rows);

    usize input_start = layout.output_rows;
    if (input_start + layout.input_rows > (usize)height) {
        input_start = (usize)height - MIN(layout.input_rows, (usize)height);
    }

    layout.visible_output_rows = MIN(input_start, (usize)height);
    layout.max_scroll          = layout.output_rows > layout.visible_output_rows
                                     ? layout.output_rows - layout.visible_output_rows
                                     : 0;
    return layout;
}

//------------------------------------------------------------------------------
// _term_console_redraw
//
// Rebuild the console window contents from output history, scrollbar, and
// input state.
//------------------------------------------------------------------------------

internal void _term_console_redraw(TermConsole* console)
{
    TermWindow* window = console->window;
    int         width  = (int)window->rect.width;
    int         height = (int)window->rect.height;

    term_window_clear(window, ' ', COLOUR_WHITE, COLOUR_BLACK);

    if (width <= 0 || height <= 0) {
        term_window_draw(window);
        return;
    }

    TermConsoleLayout layout = _term_console_layout(console);

    usize input_start        = layout.output_rows;
    if (input_start + layout.input_rows > (usize)height) {
        input_start = (usize)height - MIN(layout.input_rows, (usize)height);
    }

    console->scroll_offset = MIN(console->scroll_offset, layout.max_scroll);
    if (console->scroll_offset == 0) {
        console->auto_scroll = true;
    }
    usize output_start = layout.output_rows > layout.visible_output_rows
                             ? layout.output_rows - layout.visible_output_rows -
                                   console->scroll_offset
                             : 0;

    _term_console_draw_output(
        console, output_start, layout.visible_output_rows, layout.output_width);
    _term_console_draw_scrollbar(console, layout);
    _term_console_draw_input(console, input_start);
    term_window_draw(window);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_console_init
//
// Initialise a console over an existing window and configure its default
// colours and history size.
//------------------------------------------------------------------------------

void term_console_init(TermConsole* console,
                       TermWindow*  window,
                       u32          history_size)
{
    memset(console, 0, sizeof(*console));
    console->window              = window;
    console->history_size        = history_size;
    console->output_colour       = COLOUR_WHITE;
    console->prompt_colour       = COLOUR_BRIGHT_CYAN;
    console->input_colour        = COLOUR_BRIGHT_YELLOW;
    console->input_enabled       = true;
    console->focused             = false;
    console->auto_scroll         = true;
    console->scroll_offset       = 0;
    console->input_cursor        = 0;
    console->input_history_index = -1;
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_done
//
// Release all storage owned by the console and drop focus if it owns the host
// cursor.
//------------------------------------------------------------------------------

void term_console_done(TermConsole* console)
{
    if (g_term_focused_console == console) {
        g_term_focused_console = NULL;
        console->focused       = false;
    }

    for (usize i = 0; i < array_count(console->history); i++) {
        FREE(console->history[i].text.data);
    }
    array_done(console->history);
    array_done(console->prompt);
    array_done(console->input);
    array_done(console->pending_input);
    for (usize i = 0; i < array_count(console->input_history); i++) {
        FREE(console->input_history[i].data);
    }
    array_done(console->input_history);
    array_done(console->input_history_saved);
    memset(console, 0, sizeof(*console));
}

//------------------------------------------------------------------------------
// term_console_enable_input
//
// Enable or disable interactive input for the console.
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// term_console_set_output_colour
//
// Set the base output colour used when ANSI sequences do not override it.
//------------------------------------------------------------------------------

void term_console_set_output_colour(TermConsole* console, u32 colour)
{
    console->output_colour = colour;
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_set_prompt_colour
//
// Set the colour used for the prompt.
//------------------------------------------------------------------------------

void term_console_set_prompt_colour(TermConsole* console, u32 colour)
{
    console->prompt_colour = colour;
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_set_input_colour
//
// Set the colour used for editable input text and the input cursor.
//------------------------------------------------------------------------------

void term_console_set_input_colour(TermConsole* console, u32 colour)
{
    console->input_colour = colour;
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_set_prompt
//
// Replace the prompt text displayed before the editable input.
//------------------------------------------------------------------------------

void term_console_set_prompt(TermConsole* console, string prompt)
{
    _term_console_set_bytes(&console->prompt, prompt);
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_resize
//
// Resize the underlying window and redraw the console contents to match.
//------------------------------------------------------------------------------

void term_console_resize(TermConsole* console, TermRect new_rect)
{
    term_window_resize(console->window, new_rect);
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_clear
//
// Clear the output history and reset scrolling back to the live bottom.
//------------------------------------------------------------------------------

void term_console_clear(TermConsole* console)
{
    for (usize i = 0; i < array_count(console->history); i++) {
        FREE(console->history[i].text.data);
    }
    array_clear(console->history);
    console->auto_scroll   = true;
    console->scroll_offset = 0;
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_write
//
// Append a non-wrapping output chunk to the console history.
//------------------------------------------------------------------------------

void term_console_write(TermConsole* console, string str)
{
    _term_console_append_history(console, str, false);
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_write_cstr
//
// Append a non-wrapping C string to the console history.
//------------------------------------------------------------------------------

void term_console_write_cstr(TermConsole* console, cstr string)
{
    term_console_write(console, string_from_cstr(string));
}

//------------------------------------------------------------------------------
// term_console_write_wrap
//
// Append an output chunk that may wrap across multiple visual rows.
//------------------------------------------------------------------------------

void term_console_write_wrap(TermConsole* console, string str)
{
    _term_console_append_history(console, str, true);
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_formatv
//
// Format a non-wrapping output chunk from a `va_list`.
//------------------------------------------------------------------------------

void term_console_formatv(TermConsole* console, cstr fmt, va_list args)
{
    string formatted = string_formatv(temp_arena(), fmt, args);
    term_console_write(console, formatted);
}

//------------------------------------------------------------------------------
// term_console_format
//
// Format and append a non-wrapping output chunk.
//------------------------------------------------------------------------------

void term_console_format(TermConsole* console, cstr fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    term_console_formatv(console, fmt, args);
    va_end(args);
}

//------------------------------------------------------------------------------
// term_console_formatv_wrap
//
// Format a wrapping output chunk from a `va_list`.
//------------------------------------------------------------------------------

void term_console_formatv_wrap(TermConsole* console, cstr fmt, va_list args)
{
    string formatted = string_formatv(temp_arena(), fmt, args);
    term_console_write_wrap(console, formatted);
}

//------------------------------------------------------------------------------
// term_console_format_wrap
//
// Format and append a wrapping output chunk.
//------------------------------------------------------------------------------

void term_console_format_wrap(TermConsole* console, cstr fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    term_console_formatv_wrap(console, fmt, args);
    va_end(args);
}

//------------------------------------------------------------------------------
// term_console_send_event
//
// Feed a terminal event into the console, updating focus, scrolling, and
// input-editing state as needed.
//------------------------------------------------------------------------------

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
        if ((event.key_modifiers & TERM_KEYMOD_CTRL) != 0 &&
            (event.key == 'l' || event.key == 'L' || event.key == 12)) {
            term_console_clear(console);
            break;
        }

        if (event.key_code == TERM_KEY_HOME &&
            (event.key_modifiers & TERM_KEYMOD_CTRL) != 0) {
            TermConsoleLayout layout = _term_console_layout(console);
            console->scroll_offset   = layout.max_scroll;
            console->auto_scroll     = layout.max_scroll == 0;
            break;
        }

        if (event.key_code == TERM_KEY_END &&
            (event.key_modifiers & TERM_KEYMOD_CTRL) != 0) {
            console->scroll_offset = 0;
            console->auto_scroll   = true;
            break;
        }

        if (!console->input_enabled) {
            break;
        }

        if (event.key == '\r' || event.key == '\n') {
            _term_console_accept_input(console);
        } else if ((event.key_code == TERM_KEY_BACKSPACE) ||
                   (event.key == 8 || event.key == 127)) {
            if ((event.key_modifiers & TERM_KEYMOD_CTRL) != 0 ||
                event.key == 23) {
                usize start =
                    _term_console_word_left(console, console->input_cursor);
                _term_console_delete_range(
                    console, start, console->input_cursor);
            } else if (console->input_cursor > 0) {
                _term_console_delete_range(
                    console, console->input_cursor - 1, console->input_cursor);
            }
        } else if (event.key_code == TERM_KEY_DELETE) {
            if (console->input_cursor < array_count(console->input)) {
                _term_console_delete_range(
                    console, console->input_cursor, console->input_cursor + 1);
            }
        } else if (event.key_code == TERM_KEY_LEFT) {
            if ((event.key_modifiers & TERM_KEYMOD_CTRL) != 0) {
                console->input_cursor =
                    _term_console_word_left(console, console->input_cursor);
            } else if (console->input_cursor > 0) {
                console->input_cursor--;
            }
        } else if (event.key_code == TERM_KEY_RIGHT) {
            if ((event.key_modifiers & TERM_KEYMOD_CTRL) != 0) {
                console->input_cursor =
                    _term_console_word_right(console, console->input_cursor);
            } else if (console->input_cursor < array_count(console->input)) {
                console->input_cursor++;
            }
        } else if (event.key_code == TERM_KEY_HOME) {
            console->input_cursor = 0;
        } else if (event.key_code == TERM_KEY_END) {
            console->input_cursor = array_count(console->input);
        } else if (event.key_code == TERM_KEY_UP) {
            _term_console_history_up(console);
        } else if (event.key_code == TERM_KEY_DOWN) {
            _term_console_history_down(console);
        } else if ((u8)event.key >= 32) {
            _term_console_insert_byte(console, (u8)event.key);
        }
        break;

    case TERM_EVENT_MOUSE:
        if (event.mouse.wheel != 0) {
            TermConsoleLayout layout = _term_console_layout(console);
            if (layout.max_scroll > 0) {
                isize next_offset = (isize)console->scroll_offset;
                if (event.mouse.wheel > 0) {
                    next_offset += event.mouse.wheel;
                } else {
                    next_offset += event.mouse.wheel;
                }
                next_offset = CLAMP(next_offset, 0, (isize)layout.max_scroll);
                console->scroll_offset = (usize)next_offset;
                console->auto_scroll   = console->scroll_offset == 0;
            }
        }
        break;
    }

    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_unfocus
//
// Release focus from this console and hide the host cursor until another
// console claims it.
//------------------------------------------------------------------------------

void term_console_unfocus(TermConsole* console)
{
    if (g_term_focused_console == console) {
        g_term_focused_console = NULL;
    }
    console->focused = false;
    term_cursor_hide();
    _term_console_redraw(console);
}

//------------------------------------------------------------------------------
// term_console_get_input
//
// Return the most recently accepted input line, if one is pending.
//------------------------------------------------------------------------------

bool term_console_get_input(TermConsole* console, string* out_input)
{
    if (!console->has_pending_input) {
        return false;
    }

    if (out_input) {
        *out_input = string_from(console->pending_input,
                                 array_count(console->pending_input));
    }
    console->has_pending_input = false;
    return true;
}

//------------------------------------------------------------------------------
