//------------------------------------------------------------------------------
// Terminal ANSI utilities
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include "internal.h"

//------------------------------------------------------------------------------
// _term_ansi_palette
//
// Map a basic ANSI colour index onto the fixed RGB palette used by the term
// module.
//------------------------------------------------------------------------------

u32 _term_ansi_palette(int index)
{
    static const u32 palette[16] = {
        0xFF000000, 0xFF800000, 0xFF008000, 0xFF808000,
        0xFF000080, 0xFF800080, 0xFF008080, 0xFFC0C0C0,
        0xFF808080, 0xFFFF0000, 0xFF00FF00, 0xFFFFFF00,
        0xFF0000FF, 0xFFFF00FF, 0xFF00FFFF, 0xFFFFFFFF,
    };

    index = CLAMP(index, 0, 15);
    return palette[index];
}

//------------------------------------------------------------------------------
// _term_ansi_256_colour
//
// Convert an ANSI 256-colour palette index into an RGBA colour value.
//------------------------------------------------------------------------------

u32 _term_ansi_256_colour(int index)
{
    static const u8 cube_levels[6] = {0, 95, 135, 175, 215, 255};

    if (index < 16) {
        return _term_ansi_palette(index);
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

//------------------------------------------------------------------------------
// _term_apply_sgr
//
// Apply a parsed SGR parameter list to the current ink and paper colours.
//------------------------------------------------------------------------------

void _term_apply_sgr(int  params[],
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
            *io_ink = _term_ansi_palette(code - 30);
        } else if (code >= 90 && code <= 97) {
            *io_ink = _term_ansi_palette((code - 90) + 8);
        } else if (code >= 40 && code <= 47) {
            *io_paper = _term_ansi_palette(code - 40);
        } else if (code >= 100 && code <= 107) {
            *io_paper = _term_ansi_palette((code - 100) + 8);
        } else if (code == 39) {
            *io_ink = base_ink;
        } else if (code == 49) {
            *io_paper = base_paper;
        } else if ((code == 38 || code == 48) && i + 1 < count) {
            u32* target = code == 38 ? io_ink : io_paper;
            int  mode   = params[++i];

            if (mode == 5 && i + 1 < count) {
                *target = _term_ansi_256_colour(params[++i]);
            } else if (mode == 2 && i + 3 < count) {
                int raw_r = params[++i];
                int raw_g = params[++i];
                int raw_b = params[++i];
                int r     = CLAMP(raw_r, 0, 255);
                int g     = CLAMP(raw_g, 0, 255);
                int b     = CLAMP(raw_b, 0, 255);
                *target = term_rgb((u8)r, (u8)g, (u8)b);
            }
        }
    }
}

//------------------------------------------------------------------------------
// _term_try_parse_sgr
//
// Parse an ANSI SGR escape sequence at the current byte position and update the
// current colours if the sequence is complete.
//------------------------------------------------------------------------------

usize _term_try_parse_sgr(const u8* s,
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
            _term_apply_sgr(
                params, count, base_ink, base_paper, io_ink, io_paper);
            return (usize)((p + 1) - s);
        } else {
            return 0;
        }
    }

    return 0;
}
