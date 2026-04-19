//------------------------------------------------------------------------------
// Implementation of clipping windows
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include "internal.h"
#include <term/term.h>

//------------------------------------------------------------------------------
// _term_ansi_palette
//
// Map a basic ANSI colour index onto the terminal colour constants used by the
// framebuffer and window code.
//------------------------------------------------------------------------------

internal u32 _term_ansi_palette(int index)
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

    if (index < 0) {
        index = 0;
    } else if (index > 15) {
        index = 15;
    }
    return palette[index];
}

//------------------------------------------------------------------------------
// _term_ansi_256_colour
//
// Convert an ANSI 256-colour palette index into an RGB value.
//------------------------------------------------------------------------------

internal u32 _term_ansi_256_colour(int index)
{
    static const u8 cube_levels[6] = {0, 95, 135, 175, 215, 255};

    if (index < 0) {
        index = 0;
    }

    if (index < 16) {
        return _term_ansi_palette(index);
    }

    if (index < 232) {
        int base = index - 16;
        int r    = base / 36;
        int g    = (base / 6) % 6;
        int b    = base % 6;
        return term_rgb(cube_levels[r], cube_levels[g], cube_levels[b]);
    }

    if (index > 255) {
        index = 255;
    }

    u8 grey = (u8)(8 + ((index - 232) * 10));
    return term_rgb(grey, grey, grey);
}

//------------------------------------------------------------------------------
// _term_window_apply_sgr
//
// Apply a parsed ANSI SGR sequence to the current window write colours. Reset
// operations restore the colours that were already present in the window cells
// before the write began.
//------------------------------------------------------------------------------

internal void _term_window_apply_sgr(int  params[],
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
            continue;
        }

        if (code >= 30 && code <= 37) {
            *io_ink = _term_ansi_palette(code - 30);
            continue;
        }

        if (code >= 90 && code <= 97) {
            *io_ink = _term_ansi_palette((code - 90) + 8);
            continue;
        }

        if (code >= 40 && code <= 47) {
            *io_paper = _term_ansi_palette(code - 40);
            continue;
        }

        if (code >= 100 && code <= 107) {
            *io_paper = _term_ansi_palette((code - 100) + 8);
            continue;
        }

        if (code == 39) {
            *io_ink = base_ink;
            continue;
        }

        if (code == 49) {
            *io_paper = base_paper;
            continue;
        }

        if ((code == 38 || code == 48) && i + 1 < count) {
            u32* target = code == 38 ? io_ink : io_paper;
            int  mode   = params[++i];

            if (mode == 5 && i + 1 < count) {
                *target = _term_ansi_256_colour(params[++i]);
                continue;
            }

            if (mode == 2 && i + 3 < count) {
                int r   = CLAMP(params[++i], 0, 255);
                int g   = CLAMP(params[++i], 0, 255);
                int b   = CLAMP(params[++i], 0, 255);
                *target = term_rgb((u8)r, (u8)g, (u8)b);
                continue;
            }
        }
    }
}

//------------------------------------------------------------------------------
// _term_window_try_parse_sgr
//
// Parse an ANSI SGR escape sequence beginning at the current byte position.
// Returns the number of bytes consumed when a valid sequence is found, or 0 if
// the current bytes are not a complete SGR sequence.
//------------------------------------------------------------------------------

internal usize _term_window_try_parse_sgr(const u8* s,
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
            continue;
        }

        if (ch == ';') {
            if (count < ARRAY_COUNT(params)) {
                params[count++] = have_digit ? current : 0;
            }
            current    = 0;
            have_digit = false;
            p++;
            continue;
        }

        if (ch == 'm') {
            if (count < ARRAY_COUNT(params)) {
                params[count++] = have_digit ? current : 0;
            }
            _term_window_apply_sgr(
                params, count, base_ink, base_paper, io_ink, io_paper);
            return (usize)((p + 1) - s);
        }

        return 0;
    }

    return 0;
}

//------------------------------------------------------------------------------
// term_window_init
//
// Initialise a TermWindow with the given rectangle. The window's cells will be
// allocated and initialised to default values.
//------------------------------------------------------------------------------

void term_window_init(TermWindow* window, TermRect rect)
{
    window->rect = rect;
    array_requires_size(window->cells, rect.width * rect.height);
    term_window_clear(window, ' ', 0xFFFFFF, 0x000000);
}

//------------------------------------------------------------------------------
// term_window_done
//
// Free the resources used by a TermWindow.
//------------------------------------------------------------------------------

void term_window_done(TermWindow* window) { array_free(window->cells); }

//------------------------------------------------------------------------------
// term_window_clear
//
// Clear the whole area of a window
//------------------------------------------------------------------------------

void term_window_clear(const TermWindow* window, u32 ch, u32 ink, u32 paper)
{
    TermRect rect = term_rect(0, 0, window->rect.width, window->rect.height);
    term_window_paint_rect(window, rect, ch, ink, paper);
}

//------------------------------------------------------------------------------
// _term_get_local_rect
//
// Clip the given rect with the windows boundaries to get a local clipped rect.
// Returns false if the clipped rect is empty.  Also calculate the start index
// within the window's buffer and the stride to go from the end of one row to
// the start of another.
//------------------------------------------------------------------------------

bool _term_get_local_rect(const TermWindow* window,
                          TermRect          rect,
                          TermRect*         out_clipped_rect,
                          u32*              start_index,
                          u32*              stride)
{
    // Ensure the rectangle is clipped within the bounds of the window
    TermRect local_window_rect =
        term_rect(0, 0, window->rect.width, window->rect.height);
    TermRect clipped_rect = term_rect_intersection(rect, local_window_rect);
    if (term_rect_is_empty(clipped_rect)) {
        return false;
    }

    *out_clipped_rect = clipped_rect;

    *start_index =
        (u32)clipped_rect.y * (u32)window->rect.width + (u32)clipped_rect.x;
    *stride = (u32)(window->rect.width - clipped_rect.width);
    return true;
}

//------------------------------------------------------------------------------
// term_window_rect
//
// Fill a rectangle within the window with the given character, but do not
// change the ink or paper colours.  Do affect the colours use term_window_paint
// or term_window_paint_rect.
//------------------------------------------------------------------------------

void term_window_rect(const TermWindow* window, TermRect rect, u32 ch)
{
    TermRect clipped_rect;
    u32      start_index;
    u32      stride;
    if (!_term_get_local_rect(
            window, rect, &clipped_rect, &start_index, &stride)) {
        return;
    }

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            window->cells[start_index++].ch = ch;
        }
        start_index += stride;
    }
}

//------------------------------------------------------------------------------
// term_window_paint
//
// Paint a rectangle within the window with the given ink and paper colours, but
// do not change the character.  To change the character use term_window_rect or
// term_window_paint_rect.
//------------------------------------------------------------------------------

void term_window_paint(const TermWindow* window,
                       TermRect          rect,
                       u32               ink,
                       u32               paper)
{
    TermRect clipped_rect;
    u32      start_index;
    u32      stride;
    if (!_term_get_local_rect(
            window, rect, &clipped_rect, &start_index, &stride)) {
        return;
    }

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            TermCell* cell = &window->cells[start_index++];
            cell->ink      = ink;
            cell->paper    = paper;
        }
        start_index += stride;
    }
}

//------------------------------------------------------------------------------
// term_window_paint_rect
//
// Paint a rectangle within the window with the given character, ink and paper
// colours.
//------------------------------------------------------------------------------

void term_window_paint_rect(
    const TermWindow* window, TermRect rect, u32 ch, u32 ink, u32 paper)
{
    TermRect clipped_rect;
    u32      start_index;
    u32      stride;
    if (!_term_get_local_rect(
            window, rect, &clipped_rect, &start_index, &stride)) {
        return;
    }

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            TermCell* cell = &window->cells[start_index++];
            cell->ch       = ch;
            cell->ink      = ink;
            cell->paper    = paper;
        }
        start_index += stride;
    }
}

//------------------------------------------------------------------------------
// term_window_9slice
//
// Draw a 9-slice box within the window using the given slice characters. The
// slice characters should be in the following order:
//
//   0 1 2
//   3 4 5
//   6 7 8
//
// The corners (0, 2, 6, 8) will be drawn once. The edges (1, 3, 5, 7) will be
// repeated to fill the edges of the box. The center (4) will be repeated to
// fill the center of the box if `fill_centre` is true.
//------------------------------------------------------------------------------

void term_window_9slice(const TermWindow* window,
                        TermRect          rect,
                        cstr              slices,
                        bool              fill_centre)
{
    // Corners
    term_window_rect(window, term_rect(rect.x, rect.y, 1, 1), slices[0]);
    term_window_rect(
        window, term_rect(rect.x + rect.width - 1, rect.y, 1, 1), slices[2]);
    term_window_rect(
        window, term_rect(rect.x, rect.y + rect.height - 1, 1, 1), slices[6]);
    term_window_rect(
        window,
        term_rect(rect.x + rect.width - 1, rect.y + rect.height - 1, 1, 1),
        slices[8]);

    // Edges
    if (rect.width > 2) {
        term_window_rect(window,
                         term_rect(rect.x + 1, rect.y, rect.width - 2, 1),
                         slices[1]);
        term_window_rect(
            window,
            term_rect(rect.x + 1, rect.y + rect.height - 1, rect.width - 2, 1),
            slices[7]);
    }
    if (rect.height > 2) {
        term_window_rect(window,
                         term_rect(rect.x, rect.y + 1, 1, rect.height - 2),
                         slices[3]);
        term_window_rect(
            window,
            term_rect(rect.x + rect.width - 1, rect.y + 1, 1, rect.height - 2),
            slices[5]);
    }

    // Center
    if (fill_centre && rect.width > 2 && rect.height > 2) {
        term_window_rect(
            window,
            term_rect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2),
            slices[4]);
    }
}

//------------------------------------------------------------------------------
// term_window_write
//
// Write a string to the window on a single row, clipping the beginning or
// end of the string as necessary.
//------------------------------------------------------------------------------

void term_window_write(TermWindow* window, int x, int y, string str)
{
    const int width = (int)window->rect.width;

    if (y < 0 || y >= (int)window->rect.height) {
        return;
    }
    if (width <= 0) {
        return;
    }

    int base_x = CLAMP(x, 0, width - 1);
    u32 base_ink =
        window->cells[(usize)y * window->rect.width + (usize)base_x].ink;
    u32 base_paper =
        window->cells[(usize)y * window->rect.width + (usize)base_x].paper;
    u32       current_ink   = base_ink;
    u32       current_paper = base_paper;
    int       cx            = x;
    const u8* s             = str.data;
    const u8* end           = str.data + str.count;

    while (s < end) {
        usize sgr_bytes = _term_window_try_parse_sgr(
            s, end, base_ink, base_paper, &current_ink, &current_paper);
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
            break;
        }

        if (glyph_width == 0) {
            glyph_width = 1;
        }
        if (glyph_width > (usize)width) {
            cx += (int)glyph_width;
            continue;
        }

        if (cx + (int)glyph_width <= 0) {
            cx += (int)glyph_width;
            continue;
        }

        if (cx >= width) {
            break;
        }

        if (cx < 0) {
            cx += (int)glyph_width;
            continue;
        }
        if (cx + (int)glyph_width > width) {
            break;
        }

        usize index                = (usize)y * window->rect.width + (usize)cx;
        window->cells[index].ch    = ch;
        window->cells[index].ink   = current_ink;
        window->cells[index].paper = current_paper;

        for (usize cell = 1; cell < glyph_width && cx + (int)cell < width;
             ++cell) {
            TermCell* tail = &window->cells[index + cell];
            tail->ch       = TERM_FB_CHAR_WIDE_TAIL;
            tail->ink      = current_ink;
            tail->paper    = current_paper;
        }

        cx += (int)glyph_width;
    }
}

//------------------------------------------------------------------------------
// term_window_write_cstr
//
// Write a C-string to the window on a single row, clipping the beginning or
// end of the string as necessary.
//------------------------------------------------------------------------------

void term_window_write_cstr(TermWindow* window, int x, int y, cstr string)
{
    term_window_write(window, x, y, string_from_cstr(string));
}

//------------------------------------------------------------------------------
// term_window_formatv
//
// Format a string with the given arguments and write it to the window on a
// single row, clipping the beginning or end of the string as necessary.
//
// WARNING: uses the temp arena so make sure you reset the arena in your
// main loop using `temp_arena_reset()`.
//------------------------------------------------------------------------------

void term_window_formatv(
    TermWindow* window, int x, int y, cstr fmt, va_list args)
{
    string formatted = string_formatv(temp_arena(), fmt, args);
    term_window_write(window, x, y, formatted);
}

//------------------------------------------------------------------------------
// term_window_format
//
// Format a string with the given arguments and write it to the window on a
// single row, clipping the beginning or end of the string as necessary.
//
// WARNING: uses the temp arena so make sure you reset the arena in your main
// loop using `temp_arena_reset()`.
//------------------------------------------------------------------------------

void term_window_format(TermWindow* window, int x, int y, cstr fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    term_window_formatv(window, x, y, fmt, args);
    va_end(args);
}

//------------------------------------------------------------------------------
// term_window_draw
//
// Draw the contents of the window to the framebuffer, clipping if the window
// extends beyond the framebuffer boundaries.
//------------------------------------------------------------------------------

void term_window_draw(const TermWindow* window)
{
    i32 win_x0  = (i16)window->rect.x;
    i32 win_y0  = (i16)window->rect.y;
    i32 win_x1  = win_x0 + (i32)window->rect.width;
    i32 win_y1  = win_y0 + (i32)window->rect.height;
    i32 draw_x0 = MAX(win_x0, 0);
    i32 draw_y0 = MAX(win_y0, 0);
    i32 draw_x1 = MIN(win_x1, (i32)g_term_fb_size.width);
    i32 draw_y1 = MIN(win_y1, (i32)g_term_fb_size.height);

    if (draw_x1 <= draw_x0 || draw_y1 <= draw_y0) {
        return;
    }

    u32 draw_width    = (u32)(draw_x1 - draw_x0);
    u32 draw_height   = (u32)(draw_y1 - draw_y0);
    u32 src_row_start = (u32)(draw_y0 - win_y0) * (u32)window->rect.width +
                        (u32)(draw_x0 - win_x0);
    u32 dst_row_start = (u32)draw_y0 * (u32)g_term_fb_size.width + (u32)draw_x0;
    u32 src_stride    = (u32)window->rect.width;
    u32 dst_stride    = (u32)g_term_fb_size.width;

    for (u32 y = 0; y < draw_height; y++) {
        u32 src_index = src_row_start;
        u32 dst_index = dst_row_start;
        for (u32 x = 0; x < draw_width; x++) {
            TermCell cell              = window->cells[src_index++];
            g_term_fb_chars[dst_index] = cell.ch;
            g_term_fb_ink[dst_index]   = cell.ink;
            g_term_fb_paper[dst_index] = cell.paper;
            g_term_fb_dirty[dst_index] = 1;
            dst_index++;
        }

        src_row_start += src_stride;
        dst_row_start += dst_stride;
    }
}

//------------------------------------------------------------------------------
// term_window_resize
//
// Resize the window backbuffer, truncating and removing rows if the width or
// height reduces, or filling with spaces otherwise.  If row width reduces,
// the window array contents are copied in such a way that the beginning of
// each rows remains the same.
//
// All backbuffer adjustments are made in place with only expansion of the
// arrays happening if more cells are required than can be held in capacity.
//------------------------------------------------------------------------------

void term_window_resize(TermWindow* window, TermRect new_rect)
{
    if (new_rect.width == window->rect.width &&
        new_rect.height == window->rect.height) {
        window->rect = new_rect;
        return;
    }

    u32 old_width        = window->rect.width;
    u32 old_height       = window->rect.height;
    u32 new_width        = new_rect.width;
    u32 new_height       = new_rect.height;

    usize required_cells = (usize)new_width * (usize)new_height;
    u32   shared_width   = MIN(old_width, new_width);
    u32   shared_height  = MIN(old_height, new_height);

    array_requires_size(window->cells, required_cells);

    // Reflow the rows in place when the row stride changes. Shrinking moves
    // rows towards lower indices, so copy top-down. Growing moves rows towards
    // higher indices, so copy bottom-up.
    if (new_width < old_width) {
        for (u32 y = 0; y < shared_height; y++) {
            usize src_index = (usize)y * old_width;
            usize dst_index = (usize)y * new_width;
            memmove(&window->cells[dst_index],
                    &window->cells[src_index],
                    sizeof(TermCell) * shared_width);
        }
    } else if (new_width > old_width) {
        for (i32 y = (i32)shared_height - 1; y >= 0; y--) {
            usize src_index = (usize)y * old_width;
            usize dst_index = (usize)y * new_width;
            memmove(&window->cells[dst_index],
                    &window->cells[src_index],
                    sizeof(TermCell) * shared_width);
        }
    }

    // Update the window rect before filling to ensure the correct clipping
    // happens when clearing any new areas.
    window->rect = new_rect;

    // Clear any new areas with spaces and default colours
    if (new_width > old_width) {
        for (u16 y = 0; y < MIN(old_height, new_height); y++) {
            term_window_paint_rect(
                window,
                term_rect(old_width, y, new_width - old_width, 1),
                ' ',
                0xFFFFFF,
                0x000000);
        }
    }
    if (new_height > old_height) {
        term_window_paint_rect(
            window,
            term_rect(0, old_height, new_width, new_height - old_height),
            ' ',
            0xFFFFFF,
            0x000000);
    }
}
