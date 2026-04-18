//------------------------------------------------------------------------------
// Terminal FrameBuffer API
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include "internal.h"
#include <term/term.h>
#include <wchar.h>

//------------------------------------------------------------------------------

void _term_fb_resize(u16 width, u16 height)
{
    TermSize size                 = g_term_fb_size;
    usize    current_num_elements = size.width * size.height;
    usize    num_elements         = width * height;

    if (num_elements > current_num_elements) {
        // Need to allocate more memory
        array_requires_size(g_term_fb_chars, num_elements);
        array_requires_size(g_term_fb_ink, num_elements);
        array_requires_size(g_term_fb_paper, num_elements);
        array_requires_size(g_term_fb_dirty, num_elements);
    }

    // If the width or height as reduced, we need to truncate by repositioning
    // the rows in the array. If the have grown, we need to pad the rows out.
    // Either way, we should reorganise the elements in the array so that the 2D
    // rectangle is either grown or truncated.
    for (u16 y = 0; y < height; y++) {
        for (u16 x = 0; x < width; x++) {
            usize new_index = y * width + x;
            usize old_index = y * size.width + x;
            if (x < size.width && y < size.height) {
                // Copy existing data
                g_term_fb_chars[new_index] = g_term_fb_chars[old_index];
                g_term_fb_ink[new_index]   = g_term_fb_ink[old_index];
                g_term_fb_paper[new_index] = g_term_fb_paper[old_index];
                g_term_fb_dirty[new_index] = g_term_fb_dirty[old_index];
            } else {
                // New area, clear it
                g_term_fb_chars[new_index] = ' ';
                g_term_fb_ink[new_index]   = term_rgba(255, 255, 255, 255);
                g_term_fb_paper[new_index] = term_rgba(0, 0, 0, 255);
                g_term_fb_dirty[new_index] = 1;
            }
        }
    }

    // Update the size
    g_term_fb_size.width  = width;
    g_term_fb_size.height = height;
}

void _term_fb_done(void)
{
    array_free(g_term_fb_chars);
    array_free(g_term_fb_ink);
    array_free(g_term_fb_paper);
    array_free(g_term_fb_dirty);
}

//------------------------------------------------------------------------------

u32 term_rgb(u8 r, u8 g, u8 b)
{
    return (0xFF << 24) | (r << 16) | (g << 8) | (b << 0);
}

u32 term_rgba(u8 r, u8 g, u8 b, u8 a)
{
    return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}

u32 term_blend(u32 dest, u32 src, f32 alpha)
{
    u8 dest_r = (dest >> 16) & 0xFF;
    u8 dest_g = (dest >> 8) & 0xFF;
    u8 dest_b = (dest >> 0) & 0xFF;

    u8 src_r  = (src >> 16) & 0xFF;
    u8 src_g  = (src >> 8) & 0xFF;
    u8 src_b  = (src >> 0) & 0xFF;

    u8 out_r  = (u8)((src_r * alpha) + (dest_r * (1.0f - alpha)));
    u8 out_g  = (u8)((src_g * alpha) + (dest_g * (1.0f - alpha)));
    u8 out_b  = (u8)((src_b * alpha) + (dest_b * (1.0f - alpha)));

    return term_rgb(out_r, out_g, out_b);
}

//------------------------------------------------------------------------------

void term_fb_cls(u32 ink, u32 paper)
{
    TermSize size = g_term_fb_size;
    TermRect rect = {0, 0, size.width, size.height};
    term_fb_rect(rect, ' ', ink, paper);
}

void term_fb_clip_rect(TermRect  rect,
                       TermRect* out_clipped_rect,
                       TermRect* out_local_rect)
{
    TermSize size = g_term_fb_size;

    // Clip the rectangle to the framebuffer size
    u16 x0        = rect.x;
    u16 y0        = rect.y;
    u16 x1        = rect.x + rect.width;
    u16 y1        = rect.y + rect.height;

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > size.width) {
        x1 = size.width;
    }
    if (y1 > size.height) {
        y1 = size.height;
    }

    // Output the clipped rectangle
    out_clipped_rect->x      = x0;
    out_clipped_rect->y      = y0;
    out_clipped_rect->width  = x1 - x0;
    out_clipped_rect->height = y1 - y0;

    // Output the local rectangle
    out_local_rect->x        = x0 - rect.x;
    out_local_rect->y        = y0 - rect.y;
    out_local_rect->width    = out_clipped_rect->width;
    out_local_rect->height   = out_clipped_rect->height;
}

void term_fb_rect_ink(TermRect rect, u32 colour)
{
    TermRect clipped_rect, local_rect;
    term_fb_clip_rect(rect, &clipped_rect, &local_rect);

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            usize index = (clipped_rect.y + y) * g_term_fb_size.width +
                          (clipped_rect.x + x);
            g_term_fb_ink[index]   = colour;
            g_term_fb_dirty[index] = 1;
        }
    }
}

void term_fb_rect_paper(TermRect rect, u32 colour)
{
    TermRect clipped_rect, local_rect;
    term_fb_clip_rect(rect, &clipped_rect, &local_rect);

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            usize index = (clipped_rect.y + y) * g_term_fb_size.width +
                          (clipped_rect.x + x);
            g_term_fb_paper[index] = colour;
            g_term_fb_dirty[index] = 1;
        }
    }
}

void term_fb_rect_colour(TermRect rect, u32 ink, u32 paper)
{
    TermRect clipped_rect, local_rect;
    term_fb_clip_rect(rect, &clipped_rect, &local_rect);

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            usize index = (clipped_rect.y + y) * g_term_fb_size.width +
                          (clipped_rect.x + x);
            g_term_fb_ink[index]   = ink;
            g_term_fb_paper[index] = paper;
            g_term_fb_dirty[index] = 1;
        }
    }
}

void term_fb_rect_char(TermRect rect, u32 ch)
{
    TermRect clipped_rect, local_rect;
    term_fb_clip_rect(rect, &clipped_rect, &local_rect);

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            usize index = (clipped_rect.y + y) * g_term_fb_size.width +
                          (clipped_rect.x + x);
            g_term_fb_chars[index] = ch;
            g_term_fb_dirty[index] = 1;
        }
    }
}

void term_fb_rect(TermRect rect, u32 ch, u32 ink, u32 paper)
{
    TermRect clipped_rect, local_rect;
    term_fb_clip_rect(rect, &clipped_rect, &local_rect);

    for (u16 y = 0; y < clipped_rect.height; y++) {
        for (u16 x = 0; x < clipped_rect.width; x++) {
            usize index = (clipped_rect.y + y) * g_term_fb_size.width +
                          (clipped_rect.x + x);
            g_term_fb_chars[index] = ch;
            g_term_fb_ink[index]   = ink;
            g_term_fb_paper[index] = paper;
            g_term_fb_dirty[index] = 1;
        }
    }
}

void term_utf8_next(cstr* s, u32* out_char, usize* out_bytes, usize* out_width)
{
    const u8* ptr = (const u8*)(*s);
    u8        b0  = ptr[0];

    if (b0 == '\0') {
        *out_char  = 0;
        *out_bytes = 0;
        *out_width = 0;
        return;
    }

    u32   ch    = 0;
    usize bytes = 1;

    if (b0 < 0x80) {
        ch = b0;
    } else if ((b0 & 0xE0) == 0xC0) {
        u8 b1 = ptr[1];
        if (b1 == '\0' || (b1 & 0xC0) != 0x80) {
            goto invalid;
        }
        ch    = ((u32)(b0 & 0x1F) << 6) | (u32)(b1 & 0x3F);
        bytes = 2;
        if (ch < 0x80) {
            goto invalid;
        }
    } else if ((b0 & 0xF0) == 0xE0) {
        u8 b1 = ptr[1];
        u8 b2 = ptr[2];
        if (b1 == '\0' || b2 == '\0' || (b1 & 0xC0) != 0x80 ||
            (b2 & 0xC0) != 0x80) {
            goto invalid;
        }
        ch = ((u32)(b0 & 0x0F) << 12) | ((u32)(b1 & 0x3F) << 6) |
             (u32)(b2 & 0x3F);
        bytes = 3;
        if (ch < 0x800 || (ch >= 0xD800 && ch <= 0xDFFF)) {
            goto invalid;
        }
    } else if ((b0 & 0xF8) == 0xF0) {
        u8 b1 = ptr[1];
        u8 b2 = ptr[2];
        u8 b3 = ptr[3];
        if (b1 == '\0' || b2 == '\0' || b3 == '\0' || (b1 & 0xC0) != 0x80 ||
            (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
            goto invalid;
        }
        ch = ((u32)(b0 & 0x07) << 18) | ((u32)(b1 & 0x3F) << 12) |
             ((u32)(b2 & 0x3F) << 6) | (u32)(b3 & 0x3F);
        bytes = 4;
        if (ch < 0x10000 || ch > 0x10FFFF) {
            goto invalid;
        }
    } else {
        goto invalid;
    }

    *out_char  = ch;
    *out_bytes = bytes;

    int width  = wcwidth((wchar_t)ch);
    if (width <= 0) {
        width = 1;
    }
    *out_width = (usize)width;

    (*s) += bytes;
    return;

invalid:
    *out_char  = (u32)' ';
    *out_bytes = 1;
    *out_width = 1;
    (*s)++;
}

void term_fb_write(u16 x, u16 y, string str)
{
    TermSize  size      = g_term_fb_size;
    u16       fb_width  = size.width;
    u16       fb_height = size.height;
    u16       cx        = x;
    u16       cy        = y;
    const u8* s         = str.data;
    const u8* end       = str.data + str.count;

    if (fb_width == 0 || fb_height == 0) {
        return;
    }

    while (s < end && cy < fb_height) {
        u32   ch;
        usize bytes;
        usize width;

        u8 b0 = s[0];
        if (b0 == 0) {
            break;
        }

        if (b0 < 0x80) {
            ch    = b0;
            bytes = 1;
        } else if ((b0 & 0xE0) == 0xC0) {
            if ((usize)(end - s) < 2) {
                goto invalid_char;
            }
            u8 b1 = s[1];
            if ((b1 & 0xC0) != 0x80) {
                goto invalid_char;
            }
            ch    = ((u32)(b0 & 0x1F) << 6) | (u32)(b1 & 0x3F);
            bytes = 2;
            if (ch < 0x80) {
                goto invalid_char;
            }
        } else if ((b0 & 0xF0) == 0xE0) {
            if ((usize)(end - s) < 3) {
                goto invalid_char;
            }
            u8 b1 = s[1];
            u8 b2 = s[2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) {
                goto invalid_char;
            }
            ch = ((u32)(b0 & 0x0F) << 12) | ((u32)(b1 & 0x3F) << 6) |
                 (u32)(b2 & 0x3F);
            bytes = 3;
            if (ch < 0x800 || (ch >= 0xD800 && ch <= 0xDFFF)) {
                goto invalid_char;
            }
        } else if ((b0 & 0xF8) == 0xF0) {
            if ((usize)(end - s) < 4) {
                goto invalid_char;
            }
            u8 b1 = s[1];
            u8 b2 = s[2];
            u8 b3 = s[3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 ||
                (b3 & 0xC0) != 0x80) {
                goto invalid_char;
            }
            ch = ((u32)(b0 & 0x07) << 18) | ((u32)(b1 & 0x3F) << 12) |
                 ((u32)(b2 & 0x3F) << 6) | (u32)(b3 & 0x3F);
            bytes = 4;
            if (ch < 0x10000 || ch > 0x10FFFF) {
                goto invalid_char;
            }
        } else {
            goto invalid_char;
        }

        width = (usize)wcwidth((wchar_t)ch);
        if ((int)width <= 0) {
            width = 1;
        }

        goto process_char;

    invalid_char:
        ch    = (u32)' ';
        bytes = 1;
        width = 1;

    process_char:
        s += bytes;
        if (ch == '\n') {
            cx = x;
            cy += 1;
            continue;
        }

        if (width == 0) {
            width = 1;
        } else if (width > fb_width) {
            width = fb_width;
        }

        if (cx >= fb_width) {
            cx = x;
            cy += 1;
        }

        if (cy >= fb_height) {
            break;
        }

        if (width > (usize)(fb_width - cx)) {
            cx = x;
            cy += 1;
            if (cy >= fb_height) {
                break;
            }
            if (width > (usize)(fb_width - cx)) {
                continue;
            }
        }

        usize row_start        = (usize)cy * fb_width;
        usize index            = row_start + cx;
        g_term_fb_chars[index] = ch;
        g_term_fb_dirty[index] = 1;

        for (usize cell = 1; cell < width; ++cell) {
            u16 tail_x = (u16)(cx + cell);
            if (tail_x >= fb_width) {
                break;
            }
            usize tail_index            = row_start + tail_x;
            g_term_fb_chars[tail_index] = TERM_FB_CHAR_WIDE_TAIL;
            g_term_fb_dirty[tail_index] = 1;
        }

        cx += (u16)width;
        if (cx >= fb_width) {
            cx = x;
            cy += 1;
        }
    }
}

void term_fb_write_cstr(u16 x, u16 y, cstr str)
{
    term_fb_write(x, y, string_from_cstr(str));
}

void term_fb_formatv(u16 x, u16 y, cstr fmt, va_list args)
{
    arena_reset(&g_term_arena);
    arena_formatv(&g_term_arena, fmt, args);
    cstr output = (cstr)g_term_arena.memory;
    term_fb_write_cstr(x, y, output);
}

void term_fb_format(u16 x, u16 y, cstr fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    term_fb_formatv(x, y, fmt, args);
    va_end(args);
}

//------------------------------------------------------------------------------
// Presentation

bool _term_fb_has_dirty(void)
{
    usize count = (usize)g_term_fb_size.width * (usize)g_term_fb_size.height;
    for (usize i = 0; i < count; i++) {
        if (g_term_fb_dirty[i] != 0) {
            return true;
        }
    }
    return false;
}

void _term_fb_present_now(void)
{
    TermSize size = g_term_fb_size;
    arena_reset(&g_term_arena);

    // We go through each row to detect sequence of dirty characters and
    // consecutive ink and paper colours.
    //
    // Each sequence starts with a CURSOR_GOTO or CURSOR_RIGHT code so that
    // the cursor is at the beginning of the changed area.  Then for a
    // single sequence of same ink and paper, the ANSI attributes are
    // written to set the colours followed by the characters.
    //
    // The u32 characters are encoded into a UTF-8 sequence.
    //
    // When a row is complete, the next row is done.

    u16 last_x          = 0;
    u16 last_y          = 0;

    if (g_cursor_visible) {
        arena_format(&g_term_arena, "\x1b[?25l");
    }

    // Write home code
    arena_format(&g_term_arena, "\x1b[H");

    for (u16 y = 0; y < size.height; ++y) {
        u16 x          = 0;
        u16 base_index = y * size.width;
        while (x < size.width) {
            // Find the start of the next dirty section
            if (g_term_fb_dirty[base_index + x] == 0) {
                x++;
                continue;
            }

            // Found the start of a dirty section
            if (x != last_x || y != last_y) {
                // Move the cursor to this position
                arena_format(&g_term_arena, "\x1b[%d;%dH", y + 1, x + 1);
            }
            last_x    = x;
            last_y    = y;

            // Get the ink and paper for this section
            u32 ink   = g_term_fb_ink[base_index + x];
            u32 paper = g_term_fb_paper[base_index + x];
            arena_format(&g_term_arena,
                         "\x1b[38;2;%u;%u;%um",
                         (ink >> 16) & 0xFF,
                         (ink >> 8) & 0xFF,
                         (ink >> 0) & 0xFF);
            arena_format(&g_term_arena,
                         "\x1b[48;2;%u;%u;%um",
                         (paper >> 16) & 0xFF,
                         (paper >> 8) & 0xFF,
                         (paper >> 0) & 0xFF);

            // Output characters until the ink/paper changes or we hit a
            // clean section
            while (x < size.width && g_term_fb_dirty[base_index + x] != 0 &&
                   g_term_fb_ink[base_index + x] == ink &&
                   g_term_fb_paper[base_index + x] == paper) {
                u32 ch = g_term_fb_chars[base_index + x];

                if (ch != (u32)TERM_FB_CHAR_WIDE_TAIL) {
                    if (ch <= 0x7F) {
                        arena_format(&g_term_arena, "%c", (char)(ch & 0x7F));
                    } else if (ch <= 0x7FF) {
                        arena_format(&g_term_arena,
                                     "%c%c",
                                     (char)(0xC0 | ((ch >> 6) & 0x1F)),
                                     (char)(0x80 | (ch & 0x3F)));
                    } else if (ch <= 0xFFFF) {
                        arena_format(&g_term_arena,
                                     "%c%c%c",
                                     (char)(0xE0 | ((ch >> 12) & 0x0F)),
                                     (char)(0x80 | ((ch >> 6) & 0x3F)),
                                     (char)(0x80 | (ch & 0x3F)));
                    } else {
                        arena_format(&g_term_arena,
                                     "%c%c%c%c",
                                     (char)(0xF0 | ((ch >> 18) & 0x07)),
                                     (char)(0x80 | ((ch >> 12) & 0x3F)),
                                     (char)(0x80 | ((ch >> 6) & 0x3F)),
                                     (char)(0x80 | (ch & 0x3F)));
                    }
                }

                g_term_fb_dirty[base_index + x] = 0;
                x++;
                last_x++;
            }
        }
    }

    if (g_cursor_visible) {
        arena_format(
            &g_term_arena, "\x1b[%d;%dH", g_cursor_y + 1, g_cursor_x + 1);
        arena_format(&g_term_arena,
                     "\x1b[38;2;%u;%u;%um",
                     (g_cursor_ink >> 16) & 0xFF,
                     (g_cursor_ink >> 8) & 0xFF,
                     (g_cursor_ink >> 0) & 0xFF);
        arena_format(&g_term_arena,
                     "\x1b[48;2;%u;%u;%um",
                     (g_cursor_paper >> 16) & 0xFF,
                     (g_cursor_paper >> 8) & 0xFF,
                     (g_cursor_paper >> 0) & 0xFF);
        arena_format(&g_term_arena, "\x1b[?25h");
    } else {
        arena_format(&g_term_arena, "\x1b[?25l");
    }

    arena_null_terminate(&g_term_arena);
    cstr output = (cstr)g_term_arena.memory;
    pr("%s", output);
}
